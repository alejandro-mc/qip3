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

extern MainWindow *g_mainWindowP;
enum { WSIZE, HSIZE, STEPX, STEPY, KERNEL,KSTEPX,KSTEPY, SAMPLER,KERNELSAMPLER };
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::Convolve:
//
// Constructor.
//
Correlation::Correlation(QWidget *parent) : ImageFilter(parent)
{
	m_kernel = NULL;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::controlPanel:
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
// Convolve::applyFilter:
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
    if(m_kernelImage.size() == QSize(0,0))	return 0;
	m_width  = I1->width();
	m_height = I1->height();
	// convolve image
	if(!(gpuFlag && m_shaderFlag))
        correlation(I1, m_kernel, I2);
	else    g_mainWindowP->glw()->applyFilterGPU(m_nPasses);

	return 1;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::convolve:
//
// Convolve image I1 with convolution filter in kernel.
// Output is in I2.
//
void
Correlation::correlation(ImagePtr I1, ImagePtr kernel, ImagePtr I2)
{
//	HW_correlation(I1, kernel, I2);
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::load:
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

	// read kernel
    //m_kernel = IP_readImage(qPrintable(m_file));

    //IP_castImage(m_kernel,  BW_IMAGE, m_kernelGray);

    m_kernelImage.load(m_file);

    //IP_IPtoQImage(m_kernel, m_kernelImage);


	// update button with filename (without path)
	m_button->setText(f.fileName());
	m_button->update();

	// display kernel values

	// apply filter to source image and display result
	g_mainWindowP->preview();

	return 1;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::initShader:
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
    uniforms["u_Sampler" ] = SAMPLER;//sampler for original image
    uniforms["u_kernelSampler"] = KERNELSAMPLER; //sampler for the kernel

    //generate texture for the kernel texture
    glGenTextures(1,&m_kernelTex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,m_kernelTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glActiveTexture(GL_TEXTURE0 + 0);



    // compile shader, bind attribute vars, link shader, and initialize uniform var table
    g_mainWindowP->glw()->initShader(m_program[PASS1],
                     QString(":/hw2/vshader_correlation1.glsl"),
                     QString(":/hw2/fshader_correlation1.glsl"),
                     uniforms,
                     m_uniform[PASS1]);
    uniforms.clear();

    m_shaderFlag = true;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::gpuProgram:
//
// Active gpu program
//
void
Correlation::gpuProgram(int pass) 
{
    int w_size = m_kernelImage.width();
    int h_size = m_kernelImage.height();

    glUseProgram(m_program[PASS1].programId());

    //upload uniform values to the gpu
    glUniform1i (m_uniform[PASS1][WSIZE], w_size);
    glUniform1i (m_uniform[PASS1][HSIZE], h_size);
    glUniform1f (m_uniform[PASS1][STEPX], (GLfloat) 1.0f / m_width);
    glUniform1f (m_uniform[PASS1][STEPY], (GLfloat) 1.0f / m_height);
    glUniform1f (m_uniform[PASS1][KSTEPX],(GLfloat) 1.0f / w_size);
    glUniform1f (m_uniform[PASS1][KSTEPY],(GLfloat) 1.0f / h_size);
    glUniform1i (m_uniform[PASS1][SAMPLER], 0);// sampler to texture unit 0
    glUniform1i (m_uniform[PASS1][KERNELSAMPLER], 1);//sampler to texture unit 1 i.e. the one
                                                     //that contains the kernel values


    //pass the filter array as a texture
	m_qIm = GLWidget::convertToGLFormat(m_kernelImage);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,m_kernelTex);
    
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w_size,h_size,0,GL_RGBA,GL_UNSIGNED_BYTE,m_qIm.bits());

    glActiveTexture(GL_TEXTURE0 + 0);

}
