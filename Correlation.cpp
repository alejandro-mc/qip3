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

extern MainWindow *g_mainWindowP;
enum { WSIZE, HSIZE, STEPX, STEPY, KERNEL,KSTEPX,KSTEPY,MAGT, SAMPLER,TEMPLATESAMPLER1};
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
    m_button = new QPushButton("Load Template");

    // create label widget
    m_labelTemplate = new QLabel();
    m_labelTemplate->setText("Load a template to match");

    //create label widget to display match coordinates
    m_labelCoords = new QLabel();
    m_labelCoords->setText("");
    m_labelCoords->setFixedWidth(300);

	// assemble dialog
	vbox->addWidget(m_button);
    vbox->addWidget(m_labelTemplate);
    vbox->addWidget(m_labelCoords);
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
        correlation(I1, I2);
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
Correlation::correlation(ImagePtr I1, ImagePtr I2)
{
	int th = m_tmpltImage.height();
	int tw = m_tmpltImage.width();

	ChannelPtr<uchar> p1,end,rend,window;
	int type;
	IP_getChannel<uchar>(I1, 0, window, type);
	int total = ((m_height - th + 1) * m_width) - tw + 1;
	end = window + total;//end points to the last window location + 1

	int maxscanline = th - 1;
	unsigned int dotIT = 0;
	unsigned int dotII = 0;
	double corrVal = 0;
	double maxcorrVal = 0;
	unsigned int matchIndex = 0;
	unsigned int currentIndex = 0;

    //slide window
	for (; window < end; window += (tw-1)) {//moves window to next row
		rend = window + m_width - tw + 1;//points to last window location for a row + 1
		for (; window < rend; ++window) {//slides window horizontally
			//convolve
			dotII = 0;
			dotIT = 0;
			p1 = window;//start at the top left corner of the window
			for (int i = 0; i <= maxscanline; ++i) {
				//get pointer to the first pixel of the scanline
				QRgb* pixel = reinterpret_cast<QRgb*>(m_tmpltImage.scanLine(i));
				QRgb* slend = pixel + tw;//get scanline end

				//compute weighted sum for scanline
				for (; pixel < slend; ++pixel) {
					dotIT += qRed(*pixel) * (*p1);
					dotII += (*p1) * (*p1);
					++p1;
				}

				//advance p1 to the next scanline
				p1 = p1 - tw + m_width;
			}
			//compute the correlation value
            corrVal = dotIT / (sqrt(dotII)* m_magnitudeT);

			//update maxcorrelation and coordinates
			if (maxcorrVal < corrVal) {
				maxcorrVal = corrVal;//update correlation value
				matchIndex = currentIndex;//update match index

			}
			++currentIndex;//increment current index
		}

		currentIndex += tw -1;//first index of the next row
	}


    //display coordinates
    m_labelCoords->setText(QString("X:%1,Y:%2").arg(matchIndex % m_width).arg(matchIndex / m_width));


	//create output image
	IP_copyImageHeader(I1,I2);
	ChannelPtr<uchar> r1, r2, end1;
	int t1,t2;
	IP_getChannel<uchar>(I1, 0, r1, t1);

	IP_getChannel<uchar>(I2, 0, r2, t2);

	//reduce intensity of base image by half
	end1 = r1 + (m_width * m_height);
	for (; r1 < end1; ++r1) {
		*r2 = *r1 >> 1;//reduce by half
		++r2;
	}

	//superimpose template over the base image/////////////////

	//reset the image 2 pointer
	IP_getChannel<uchar>(I2, 0, r2, t2);
	
	//position pointer at the match index
	r2 += matchIndex;

	for (int i = 0; i <= maxscanline; ++i) {
		//get pointer to the first pixel of the scanline
		QRgb* pixel = reinterpret_cast<QRgb*>(m_tmpltImage.scanLine(i));
		QRgb* slend = pixel + tw;//get scanline end

		for (; pixel < slend; ++pixel) {
			*r2 = *r2 + (qRed(*pixel) >> 1);
			++r2;
		}

		//advance r2 to the next scanline
		r2 = r2 - tw + m_width;
	}

    
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

    //display template
    m_labelTemplate->setPixmap(QPixmap::fromImage(m_tmpltImage));

    //pass the template texture to the GPU
    m_qIm = GLWidget::convertToGLFormat(m_tmpltImage);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,m_tmpltTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RED,m_tmpltImage.width(),
                 m_tmpltImage.height(),0,GL_RGBA,GL_UNSIGNED_BYTE,m_qIm.bits());

    glActiveTexture(GL_TEXTURE0 + 0);

    //initialize the base image with and height
	ImagePtr Isrc = g_mainWindowP->imageSrc();
	m_width = Isrc->width();
	m_height = Isrc->height();

    //allocate frame buffer textures
    glBindFramebuffer(GL_FRAMEBUFFER, m_corrValsFBO);
    glBindTexture(GL_TEXTURE_2D, m_corrValsText);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_width, m_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                   GL_TEXTURE_2D, m_corrValsText, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);//unbind frame buffer
    glBindTexture(GL_TEXTURE_2D, 0);//unbind texture

	// update button with filename (without path)
	m_button->setText(f.fileName());
	m_button->update();


    int th = m_tmpltImage.height();
    int tw = m_tmpltImage.width();
    int maxscanline = th - 1;

    unsigned int dotTT=0;
    //compute magnitude of the template
    for (int i = 0; i <= maxscanline; ++i) {
        //get pointer to the first pixel of the scanline
        QRgb* pixel = reinterpret_cast<QRgb*>(m_tmpltImage.scanLine(i));
        QRgb* slend = pixel + tw;//get scanline end
        for (; pixel < slend; ++pixel) {
            dotTT += qRed(*pixel) * qRed(*pixel);
        }
    }
    m_magnitudeT = sqrt(dotTT);



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
    uniforms["u_magnitudeT"] = MAGT;//magnitude of the template
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
    int tw = m_tmpltImage.width();
    int th = m_tmpltImage.height();

    glUseProgram(m_program[PASS1].programId());

    //upload uniform values to the gpu
    glUniform1i (m_uniform[PASS1][WSIZE], tw);
    glUniform1i (m_uniform[PASS1][HSIZE], th);
    glUniform1f (m_uniform[PASS1][STEPX], (1.0 / (float)m_width));
    glUniform1f (m_uniform[PASS1][STEPY], (1.0 / (float)m_height));
    glUniform1f (m_uniform[PASS1][KSTEPX],(1.0 / (float)tw));
    glUniform1f (m_uniform[PASS1][KSTEPY],(1.0 / (float)th));
    glUniform1f (m_uniform[PASS1][MAGT],m_magnitudeT/255.0);
    glUniform1i (m_uniform[PASS1][SAMPLER], 0);// sampler to texture unit 0
    glUniform1i (m_uniform[PASS1][TEMPLATESAMPLER1], 1);//sampler to texture unit 1 i.e. the one
                                                        //that contains the template values

    //bind frame buffer and texture
    glBindFramebuffer(GL_FRAMEBUFFER, m_corrValsFBO);
    glBindTexture(GL_TEXTURE_2D, m_corrValsText);

    //bind the input texture
    glBindTexture(GL_TEXTURE_2D,g_mainWindowP->glw()->getInTexture());

    //draw call to have correlation values written to frame buffer
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei) 4);
	
    //read correlation values from GPU
    m_corrValues = new float[m_width*m_height];

    //glReadPixels will read the values bound to the frame buffer
    //starting from the bottom row
    glReadPixels(0,0,m_width,m_height,GL_RED,GL_FLOAT,m_corrValues);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);//unbind frame buffer

    //find maximum correlation value
    float* p    = m_corrValues;
    float* last = m_corrValues+(m_width*m_height);
    float  largest=0;
    unsigned int largestLocation=0;
    unsigned int pI=0;//pixel index

    while (p<last){
        if(*p > largest){
            largest = *p;
            largestLocation = pI;
        }
        ++p;
        ++pI;
    }

    //get coordinates
    int ycoord =0;
    int xcoord =0;
    //correlation values are shifted by one in the x and y direction
    //therefore I added +1 to compensate
    //this must be due to the way the textures are sampled and mapped
    //must correct this to get rid of the unintuitive +1
    ycoord = (largestLocation / m_width) + 1;
    xcoord = (largestLocation % m_width) + 1;

    //display coordinates
    m_labelCoords->setText(QString("X:%1,Y:%2").arg(xcoord).arg(m_height - (ycoord + th)));


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
    glUniform1f(m_uniform[PASS2][TIRW], (GLfloat)(tw / (float)m_width));
    glUniform1f(m_uniform[PASS2][TIRH], (GLfloat)(th / (float)m_height));
    glUniform1f(m_uniform[PASS2][TMPLTX], (GLfloat)(xcoord / (float)m_width));
    glUniform1f(m_uniform[PASS2][TMPLTY], (GLfloat)(ycoord / (float)m_height));
	glUniform1i(m_uniform[PASS2][SAMPLER2], 0);
    glUniform1i(m_uniform[PASS2][TEMPLATESAMPLER2], 1);
    //the draw call for this second program happens in GLWidget.cpp


}
