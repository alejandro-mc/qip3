// ======================================================================
// IMPROC: Image Processing Software Package
// Copyright (C) 2016 by George Wolberg
//
// Correlation.cpp - Correlation widget.
//
// Written by: George Wolberg, 2016
// Modified by: Alejandro Morejon Cortina, 2016
// ======================================================================

#include "MainWindow.h"
#include "Correlation.h"
//#include "hw2/HW_correlation.cpp"

#define FBODIM 1
extern MainWindow *g_mainWindowP;
enum { WSIZE, HSIZE, STEPX, STEPY, KERNEL,KSTEPX,KSTEPY, SAMPLER,TEMPLATESAMPLER1};
enum {TIRW,TIRH,TMPLTX,TMPLTY,SAMPLER2,TEMPLATESAMPLER2};//TIR stands for template to image ratio
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Correlation::Correlation:
//
// Constructor.
//
Correlation::Correlation(QWidget *parent) : ImageFilter(parent)
{
	m_kernel = NULL;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Correlation::controlPanel:
//
// Create group box for control panel.
//
QGroupBox*
Correlation::controlPanel()
{
	// init group box
    m_ctrlGrp = new QGroupBox("Correlation");

	// layout for assembling filter widget
	QVBoxLayout *vbox = new QVBoxLayout;

	// create file pushbutton
	m_button = new QPushButton("File");

	// create text edit widget
	m_values = new QTextEdit();
	m_values->setReadOnly(1);

	// assemble dialog
	vbox->addWidget(m_button);
	vbox->addWidget(m_values);
	m_ctrlGrp->setLayout(vbox);

	// init signal/slot connections
	connect(m_button, SIGNAL(clicked()), this, SLOT(load()));

	return(m_ctrlGrp);
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Correlation::applyFilter:
//
// Run filter on the image, transforming I1 to I2.
// Overrides ImageFilter::applyFilter().
// Return 1 for success, 0 for failure.
//
bool
Correlation::applyFilter(ImagePtr I1, bool gpuFlag, ImagePtr I2)
{
	// error checking
	if(I1.isNull())		return 0;
    if(m_tmpltImage.size() == QSize(0,0))	return 0;
	m_width  = I1->width();
	m_height = I1->height();
    // Correlation image
	if(!(gpuFlag && m_shaderFlag))
        correlation(I1, m_kernel, I2);
    else    g_mainWindowP->glw()->applyFilterGPU(m_nPasses);

	return 1;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Correlation::Correlation:
//
// Correlation image I1 with convolution filter in kernel.
// Output is in I2.
//
void
Correlation::correlation(ImagePtr I1, ImagePtr kernel, ImagePtr I2)
{
//	HW_correlation(I1, kernel, I2);
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Correlation::load:
//
// Slot to load filter kernel from file.
//
int
Correlation::load()
{
	QFileDialog dialog(this);

	// open the last known working directory
	if(!m_currentDir.isEmpty())
		dialog.setDirectory(m_currentDir);

	// display existing files and directories
	dialog.setFileMode(QFileDialog::ExistingFile);

	// invoke native file browser to select file
	m_file =  dialog.getOpenFileName(this,
        "Open File", m_currentDir,
        "Images (*.JPG);;All files (*)");

	// verify that file selection was made
	if(m_file.isNull()) return 0;

	// save current directory
	QFileInfo f(m_file);
	m_currentDir = f.absolutePath();

    // read template
    m_tmpltImage.load(m_file);

    //convert image to grayscale
    for(int i = 0; i<m_tmpltImage.height();++i){
        QRgb* pixel = reinterpret_cast<QRgb*>(m_tmpltImage.scanLine(i));
        QRgb* end   = pixel + m_tmpltImage.width();
        for(; pixel < end;++pixel){
            int gray = qGray(*pixel);
            *pixel   = QColor(gray,gray,gray).rgb();
        }
    }

    //pass the template texture to the GPU
    m_qIm = GLWidget::convertToGLFormat(m_tmpltImage);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,m_tmpltTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RED,m_tmpltImage.width(),
                 m_tmpltImage.height(),0,GL_RGBA,GL_UNSIGNED_BYTE,m_qIm.bits());

    glActiveTexture(GL_TEXTURE0 + 0);

	// update button with filename (without path)
	m_button->setText(f.fileName());
	m_button->update();


    // display template values

	// apply filter to source image and display result
	g_mainWindowP->preview();

	return 1;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Correlation::initShader:
//
// init shader program and parameters.
//
void
Correlation::initShader() 
{
    m_nPasses = 1;
    // initialize GL function resolution for current context
    initializeGLFunctions();

    UniformMap uniforms;

    // init uniform hash table based on uniform variable names and location IDs
    uniforms["u_Wsize"   ] = WSIZE;//filter width
    uniforms["u_Hsize"   ] = HSIZE;//filter height
    uniforms["u_StepX"   ] = STEPX;//horizontal step
    uniforms["u_StepY"   ] = STEPY;//vertical step
    uniforms["u_KStepX"  ] = KSTEPX;//horizontal step for kernel texture
    uniforms["u_KStepY"  ] = KSTEPY;//vertical step for kernel texture
    uniforms["u_Sampler" ] = SAMPLER;//sampler for base image
    uniforms["u_templateSampler"] = TEMPLATESAMPLER1; //sampler for the kernel

    //generate texture for the kernel texture
    glGenTextures(1,&m_tmpltTex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,m_tmpltTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    //make sure that the border color is black for this texture
    float borderColor [] = {0.0f,0.0f,0.0f,0.0f};
    glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,borderColor);
    glActiveTexture(GL_TEXTURE0 + 0);

    //generate temporary frame buffer
    glGenFramebuffers(1,&m_corrValsFBO);
    glGenTextures(1, &m_corrValsText);

    //set up the buffer and texture
    glBindFramebuffer(GL_FRAMEBUFFER, m_corrValsFBO);
    glBindTexture(GL_TEXTURE_2D, m_corrValsText);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);//unbind frame buffer
    glBindTexture(GL_TEXTURE_2D, 0);//unbind texture



    // compile shader, bind attribute vars, link shader, and initialize uniform var table
    g_mainWindowP->glw()->initShader(m_program[PASS1],
                     QString(":/vshader_passthrough.glsl"),
                     QString(":/hw2/fshader_correlation1.glsl"),
                     uniforms,
                     m_uniform[PASS1]);
    uniforms.clear();


    //init shader to display results
    uniforms["u_tmpltToImgRatioWidth"] = TIRW;
    uniforms["u_tmpltToImgRatioHeight"]= TIRH;
    uniforms["u_tmpltX" ] = TMPLTX;
    uniforms["u_tmpltY" ] = TMPLTY;
    uniforms["u_Sampler" ] = SAMPLER2;//sampler for original image
    uniforms["u_templateSampler"] = TEMPLATESAMPLER2; //sampler for the kernel

    // compile shader, bind attribute vars, link shader, and initialize uniform var table
    g_mainWindowP->glw()->initShader(m_program[PASS2],
                     QString(":/vshader_passthrough.glsl"),
                     QString(":/hw2/fshader_correlation2.glsl"),
                     uniforms,
                     m_uniform[PASS2]);
    uniforms.clear();

    m_shaderFlag = true;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Correlation::gpuProgram:
//
// Active gpu program
//
void
Correlation::gpuProgram(int pass) 
{
    int w_size = m_tmpltImage.width();
    int h_size = m_tmpltImage.height();

    glUseProgram(m_program[PASS1].programId());

    //upload uniform values to the gpu
    glUniform1i (m_uniform[PASS1][WSIZE], w_size);
    glUniform1i (m_uniform[PASS1][HSIZE], h_size);
    glUniform1f (m_uniform[PASS1][STEPX], (GLfloat) 1.0f / m_width);
    glUniform1f (m_uniform[PASS1][STEPY], (GLfloat) 1.0f / m_height);
    glUniform1f (m_uniform[PASS1][KSTEPX],(GLfloat) 1.0f / w_size);
    glUniform1f (m_uniform[PASS1][KSTEPY],(GLfloat) 1.0f / h_size);
    glUniform1i (m_uniform[PASS1][SAMPLER], 0);// sampler to texture unit 0
    glUniform1i (m_uniform[PASS1][TEMPLATESAMPLER1], 1);//sampler to texture unit 1 i.e. the one
                                                        //that contains the template values

    //allocate frame buffer textures
    glBindFramebuffer(GL_FRAMEBUFFER, m_corrValsFBO);
    glBindTexture(GL_TEXTURE_2D, m_corrValsText);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_width, m_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                   GL_TEXTURE_2D, m_corrValsText, 0);

    //bind the input texture
    glBindTexture(GL_TEXTURE_2D,g_mainWindowP->glw()->getInTexture());

    //draw arrays
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei) 4);
	
    //read correlation values from GPU
    m_corrValues = new float[m_width*m_height*FBODIM];

    //glReadPixels will read the values bound to the frame buffer
    //starting from the bottom row
    glReadPixels(0,0,m_width,m_height,GL_RED,GL_FLOAT,m_corrValues);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);//unbind frame buffer

    //find maximum correlation value
    float* p    = m_corrValues;
    float* last = m_corrValues+(m_width*m_height*FBODIM);
    float  largest=0;
    unsigned int largestLocation=0;
    unsigned int pI=0;//pixel index

    while (p<last){
        if(*p > largest){
            largest = *p;
            largestLocation = pI;
        }
        p+=FBODIM;
        ++pI;
    }

    //get coordinates
    int ycoord =0;
    int xcoord =0;
    ycoord = (largestLocation / m_width) + 1;
    xcoord = largestLocation % m_width;

    //delete corr values
    delete [] m_corrValues;

    //at this point we have the matching coordinates
    //use a different glprogram to display the template over the original image
    glUseProgram(m_program[PASS2].programId());

    //rebind pass 1 framebuffer that was bound in GLWidget
    glBindFramebuffer(GL_FRAMEBUFFER,g_mainWindowP->glw()->getFBO(PASS1));

    //bind the input texture
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,m_tmpltTex);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D,g_mainWindowP->glw()->getInTexture());


    //set uniform values
    glUniform1f(m_uniform[PASS2][TIRW], (GLfloat)(w_size / (float)m_width));
    glUniform1f(m_uniform[PASS2][TIRH], (GLfloat)(h_size / (float)m_height));
	glUniform1f(m_uniform[PASS2][TMPLTX], (GLfloat)(xcoord / (float)m_width));
	glUniform1f(m_uniform[PASS2][TMPLTY], (GLfloat)(ycoord / (float)m_height));
	glUniform1i(m_uniform[PASS2][SAMPLER2], 0);
    glUniform1i(m_uniform[PASS2][TEMPLATESAMPLER2], 1);
    //the draw call for this second program happens in GLWidget.cpp


}
