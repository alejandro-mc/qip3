// ======================================================================
// IMPROC: Image Processing Software Package
// Copyright (C) 2016 by George Wolberg
//
// Convolve.cpp - Convolve widget.
//
// Written by: George Wolberg, 2016
// ======================================================================

#include "MainWindow.h"
#include "Convolve.h"

extern MainWindow *g_mainWindowP;
enum { WSIZE, HSIZE, STEPX, STEPY, KERNEL,KSTEPX,KSTEPY, SAMPLER,KERNELSAMPLER };
enum {PROGTEX,PROGARRAY};
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::Convolve:
//
// Constructor.
//
Convolve::Convolve(QWidget *parent) : ImageFilter(parent)
{
	m_kernel = NULL;
    m_usingarray = true;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::controlPanel:
//
// Create group box for control panel.
//
QGroupBox*
Convolve::controlPanel()
{
	// init group box
	m_ctrlGrp = new QGroupBox("Convolve");

	// layout for assembling filter widget
	QVBoxLayout *vbox = new QVBoxLayout;

	// create file pushbutton
	m_button = new QPushButton("File");

	// create text edit widget
	m_values = new QTextEdit();
	m_values->setReadOnly(1);

    // create checkbox
    m_checkBox = new QCheckBox("Toggle kernel mode (array/texture).");
    m_checkBox->setCheckState(Qt::Checked);

	// assemble dialog
	vbox->addWidget(m_button);
	vbox->addWidget(m_values);
    vbox->addWidget(m_checkBox);
	m_ctrlGrp->setLayout(vbox);

	// init signal/slot connections
	connect(m_button, SIGNAL(clicked()), this, SLOT(load()));
    connect(m_checkBox,SIGNAL(stateChanged(int)),SLOT(kernelMode(int)));

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
Convolve::applyFilter(ImagePtr I1, bool gpuFlag, ImagePtr I2)
{
	// error checking
	if(I1.isNull())		return 0;
	if(m_kernel.isNull())	return 0;
	m_width  = I1->width();
	m_height = I1->height();
	// convolve image
	if(!(gpuFlag && m_shaderFlag))
		convolve(I1, m_kernel, I2);
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
Convolve::convolve(ImagePtr I1, ImagePtr kernel, ImagePtr I2)
{
    //I did not write this implementation
    //It was provided to us for the purpose of
    //running time comparison with the GPU version

    // kernel dimensions
        int ww = kernel->width ();
        int hh = kernel->height();

        // error checking: must use odd kernel dimensions
        if(!(ww%2 && hh%2)) {
            fprintf(stderr, "IP_convolve: kernel size must be odd\n");
            return;
        }

        // input image dimensions
        IP_copyImageHeader(I1, I2);
        int w = I1->width ();
        int h = I1->height();

        // clear offset; restore later
        int xoffset = I1->xoffset();
        int yoffset = I1->yoffset();
        I1->setXOffset(0);
        I1->setYOffset(0);

        // pad image to handle border problems
        int	 padnum[4];
        ImagePtr Isrc;
        padnum[0] = padnum[2] = ww/2;		// left and right  padding
        padnum[1] = padnum[3] = hh/2;		// top  and bottom padding
        IP_pad(I1, padnum, REPLICATE, Isrc);	// replicate border

        // restore offsets
        I1->setXOffset(xoffset);
        I1->setYOffset(yoffset);

        // cast kernel into array weight (of type float)
        ImagePtr Iweights;
        IP_castChannelsEq(kernel, FLOAT_TYPE, Iweights);
        ChannelPtr<float> wts = Iweights[0];

        ImagePtr I1f, I2f;
        if(I1->maxType() > UCHAR_TYPE) {
            I1f = IP_allocImage(w+ww-1, h+hh-1, FLOATCH_TYPE);
            I2f = IP_allocImage(w, h, FLOATCH_TYPE);
        }

        int	t;
        float	sum;
        ChannelPtr<uchar> p1, p2, in, out;
        ChannelPtr<float> f1, f2, wt;
        for(int ch=0; IP_getChannel(Isrc, ch, p1, t); ch++) {
            IP_getChannel(I2, ch, p2, t);
            if(t == UCHAR_TYPE) {
                out = p2;
                for(int y=0; y<h; y++) {	// visit rows
                   for(int x=0; x<w; x++) {	// slide window
                    sum = 0;
                    in  = p1 + y*(w+ww-1) + x;
                    wt  = wts;
                    for(int i=0; i<hh; i++) {	// convolution
                        for(int j=0; j<ww; j++)
                            sum += (wt[j]*in[j]);
                        in += (w+ww-1);
                        wt +=  ww;
                    }
                    *out++ = (int) (CLIP(sum, 0, MaxGray));
                   }
                }
                continue;
            }
            IP_castChannel(Isrc, ch, I1f, 0, FLOAT_TYPE);
            f2 = I2f[0];
            for(int y=0; y<h; y++) {		// visit rows
               for(int x=0; x<w; x++) {		// slide window
                sum = 0;
                f1 = I1f[0];
                f1 += y*(w+ww-1) + x;
                wt = wts;
                for(int i=0; i<hh; i++) {	// convolution
                    for(int j=0; j<ww; j++)
                        sum += (wt[j]*f1[j]);
                    f1 += (w+ww-1);
                    wt +=  ww;
                }
                *f2++ = sum;
               }
            }
            IP_castChannel(I2f, 0, I2, ch, t);
        }
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::load:
//
// Slot to load filter kernel from file.
//
int
Convolve::load()
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
		"Images (*.AF);;All files (*)");

	// verify that file selection was made
	if(m_file.isNull()) return 0;

	// save current directory
	QFileInfo f(m_file);
	m_currentDir = f.absolutePath();

	// read kernel
	m_kernel = IP_readImage(qPrintable(m_file));

	// init vars
	int w = m_kernel->width ();
	int h = m_kernel->height();

	// update button with filename (without path)
	m_button->setText(f.fileName());
	m_button->update();

	// declarations
	int type;
	ChannelPtr<float> p;
	QString s;

	// get pointer to kernel values
	IP_getChannel(m_kernel, 0, p, type);

	// display kernel values
	m_values->clear();			// clear text edit field (kernel values)
	for(int y=0; y<h; y++) {		// process all kernel rows
		s.clear();			// clear string
		for(int x=0; x<w; x++)		// append kernel values to string
			s.append(QString("%1   ").arg(*p++));
		m_values->append(s);		// append string to text edit widget
	}

    //////Loading kernel texture///////////////////////
    int w_size = m_kernel->width();
    int h_size = m_kernel->height();

    //pass the filter array as a texture
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,m_kernelTex);

    //using red chanel as texture data
    ChannelPtr<float> red;
    //int type;
    IP_getChannel(m_kernel,0, red, type);//chanel 0 for red

    //here we load the kernel image as an GL_R32F we only need one value
    //per pixel and we want the value to be a 32 bit float
    glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,w_size,h_size,0,GL_RED,GL_FLOAT,red.buf());

    glActiveTexture(GL_TEXTURE0);
    //end of loading kernel texture


	// apply filter to source image and display result
	g_mainWindowP->preview();

	return 1;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::kernelMode:
//
// Slot to toggle between texture kernel or array kernel.
//
void
Convolve::kernelMode(int state)
{
    if(state == Qt::Checked){
        m_usingarray = true;
    }else
    {
        m_usingarray = false;
    }

    // apply filter to source image and display result
    g_mainWindowP->preview();
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::initShader:
//
// init shader program and parameters.
//
void
Convolve::initShader() 
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
    uniforms["u_Sampler" ] = SAMPLER;
    uniforms["u_kernelSampler"] = KERNELSAMPLER; //sampler for the kernel

    //generate texture for the kernel texture
    glGenTextures(1,&m_kernelTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,m_kernelTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glActiveTexture(GL_TEXTURE0);



    // compile shader, bind attribute vars, link shader, and initialize uniform var table
    g_mainWindowP->glw()->initShader(m_program[PROGTEX],
                     QString(":/vshader_passthrough.glsl"),
                     QString(":/hw2/fshader_convolve_texture.glsl"),
                     uniforms,
                     m_uniform[PROGTEX]);
    uniforms.clear();


    //initialize program that works with uniform array
    // init uniform hash table based on uniform variable names and location IDs
    uniforms["u_Wsize"   ] = WSIZE;//filter width
    uniforms["u_Hsize"   ] = HSIZE;//filter height
    uniforms["u_StepX"   ] = STEPX;//horizontal step
    uniforms["u_StepY"   ] = STEPY;//vertical step
    uniforms["u_Kernel"  ] = KERNEL;//horizontal step for kernel texture
    uniforms["u_Sampler" ] = SAMPLER;

    // compile shader, bind attribute vars, link shader, and initialize uniform var table
    g_mainWindowP->glw()->initShader(m_program[PROGARRAY],
                     QString(":/vshader_passthrough.glsl"),
                     QString(":/hw2/fshader_convolve_array.glsl"),
                     uniforms,
                     m_uniform[PROGARRAY]);


    uniforms.clear();

    m_shaderFlag = true;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Convolve::gpuProgram:
//
// Active gpu program
//
void
Convolve::gpuProgram(int pass) 
{
    int w_size = m_kernel->width();
    int h_size = m_kernel->height();

    if(!m_usingarray){
        glUseProgram(m_program[PROGTEX].programId());

        //upload uniform values to the gpu
        glUniform1i (m_uniform[PROGTEX][WSIZE], w_size);
        glUniform1i (m_uniform[PROGTEX][HSIZE], h_size);
        glUniform1f (m_uniform[PROGTEX][STEPX], (GLfloat) 1.0f / m_width);
        glUniform1f (m_uniform[PROGTEX][STEPY], (GLfloat) 1.0f / m_height);
        glUniform1f (m_uniform[PROGTEX][KSTEPX],(GLfloat) 1.0f / w_size);
        glUniform1f (m_uniform[PROGTEX][KSTEPY],(GLfloat) 1.0f / h_size);
        glUniform1i (m_uniform[PROGTEX][SAMPLER], 0);// sampler to texture unit 0
        glUniform1i (m_uniform[PROGTEX][KERNELSAMPLER], 1);//sampler to texture unit 1 i.e. the one
                                                           //that contains the kernel values

    }else{
        glUseProgram(m_program[PROGARRAY].programId());

        //using red chanel as texture data
        ChannelPtr<float> red;
        int type;
        IP_getChannel(m_kernel,0, red, type);//chanel 0 for red

        //upload uniform values to the gpu
        glUniform1i (m_uniform[PROGARRAY][WSIZE], w_size);
        glUniform1i (m_uniform[PROGARRAY][HSIZE], h_size);
        glUniform1f (m_uniform[PROGARRAY][STEPX], (GLfloat) 1.0f / m_width);
        glUniform1f (m_uniform[PROGARRAY][STEPY], (GLfloat) 1.0f / m_height);
        glUniform1fv(m_uniform[PROGARRAY][KERNEL],w_size*h_size, red.buf());
        glUniform1i (m_uniform[PROGARRAY][SAMPLER], 0);// sampler to texture unit 0
    }

}
