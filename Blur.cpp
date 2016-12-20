// ======================================================================
// IMPROC: Image Processing Software Package
// Copyright (C) 2016 by George Wolberg
//
// Blur.cpp - Blur widget.
//
// Written by: George Wolberg, 2016
// ======================================================================

#include "MainWindow.h"
#include "Blur.h"

#define SINGLE_PASS 2
extern MainWindow *g_mainWindowP;
enum { WSIZE, HSIZE, STEP,STEPX,STEPY, SAMPLER };

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::Blur:
//
// Constructor.
//
Blur::Blur(QWidget *parent) : ImageFilter(parent)
{}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::controlPanel:
//
// Create group box for control panel.
//
QGroupBox*
Blur::controlPanel()
{
	// init group box
	m_ctrlGrp = new QGroupBox("Blur");

	// layout for assembling filter widget
	QGridLayout *layout = new QGridLayout;

	// alloc array of labels
	QLabel *label[2];

	// create sliders and spinboxes
	for(int i=0; i<2; i++) {
		// create label[i]
		label[i] = new QLabel(m_ctrlGrp);
		if(!i) label[i]->setText("Width");
		else   label[i]->setText("Height");

		// create slider
		m_slider [i] = new QSlider(Qt::Horizontal, m_ctrlGrp);
		m_slider [i]->setRange(1, 30);
		m_slider [i]->setValue(3);
		m_slider [i]->setSingleStep(2);
		m_slider [i]->setTickPosition(QSlider::TicksBelow);
		m_slider [i]->setTickInterval(5);

		// create spinbox
		m_spinBox[i] = new QSpinBox(m_ctrlGrp);
		m_spinBox[i]->setRange(1, 30);
		m_spinBox[i]->setValue(3);
		m_spinBox[i]->setSingleStep(2);

		// assemble dialog
		layout->addWidget(label    [i], i, 0);
		layout->addWidget(m_slider [i], i, 1);
		layout->addWidget(m_spinBox[i], i, 2);
	}

	// create checkbox
	m_checkBox = new QCheckBox("Lock filter dimensions");
	m_checkBox->setCheckState(Qt::Checked);

	// add checkbox to layout
    layout->addWidget(m_checkBox, 2, 1, Qt::AlignLeft);

    //create checkbox to chose between single and two pass blur
    m_checkBoxPasses = new QCheckBox("Single Pass");
    m_checkBox->setCheckState(Qt::Unchecked);
    layout->addWidget(m_checkBoxPasses,2,2,Qt::AlignLeft);

	// init signal/slot connections
	connect(m_slider [0], SIGNAL(valueChanged(int)), this, SLOT(changeFilterW(int)));
	connect(m_spinBox[0], SIGNAL(valueChanged(int)), this, SLOT(changeFilterW(int)));
	connect(m_slider [1], SIGNAL(valueChanged(int)), this, SLOT(changeFilterH(int)));
	connect(m_spinBox[1], SIGNAL(valueChanged(int)), this, SLOT(changeFilterH(int)));
	connect(m_checkBox  , SIGNAL(stateChanged(int)), this, SLOT(setLock(int)));
    connect(m_checkBoxPasses  , SIGNAL(stateChanged(int)), this, SLOT(setPasses(int)));

	// assign layout to group box
	m_ctrlGrp->setLayout(layout);

	return(m_ctrlGrp);
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::applyFilter:
//
// Run filter on the image, transforming I1 to I2.
// Overrides ImageFilter::applyFilter().
// Return 1 for success, 0 for failure.
//
bool
Blur::applyFilter(ImagePtr I1, bool gpuFlag, ImagePtr I2)
{
	// error checking
	if(I1.isNull()) return 0;

	// collect parameters
	int w = m_slider[0]->value();	// filter width
	int h = m_slider[1]->value();	// filter height

	m_width  = I1->width();
	m_height = I1->height();
	// apply blur filter
	if(!(gpuFlag && m_shaderFlag))
		blur(I1, w, h, I2);	// apply CPU based filter
	else    g_mainWindowP->glw()->applyFilterGPU(m_nPasses);

	return 1;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::blur:
//
// Blur image I1 with a box filter (unweighted averaging).
// The filter has width w and height h.
// Output is in I2.
//
void
Blur::blur(ImagePtr I1, int w, int h, ImagePtr I2)
{
    //force w and h to be odd
    w = w | 0x1;
    h = h | 0x1;

    IP_copyImageHeader(I1,I2);
    int width = I1->width();
    int height = I1->height();
    int total = width * height;
    int neighborhood_size = w*h;

    int * tmpImg = new int[total];//temporary image to hold the horizontal pass values
    int * tmpImgStart = tmpImg;//beginig address of temp image
    int * tmpImgrowend;//points to the end of the current tmpImg row
    int * tmpImgColptr;//points to the current column current column of the tmpImg being
                       //being procesed during vertical pass

    int * tmpImgEnd = tmpImgStart + total;//points to 1 int past the end of tmpImg

    int type;

    //p1: is the pointer to the current channel in the input image
    //p3: is the ponter to the current channel in the output image
    //p3colptr: int the current channel points to the columns of the output image
    //          during vertical pass when averaging for pixels in the tmpImg
    //          is finished and the result copied to the output image
    //rowend: for each channel marks the end of the current row in the input image
    //endd: for each channel marks the end of the channel in the input image
    ChannelPtr<uchar> p1, p3,p3colptr,rowend, endd;
    for(int ch = 0; IP_getChannel(I1, ch, p1, type); ch++) {
        IP_getChannel(I2,ch,p3,type);
        endd = p1+total;


        //blur rows///////////////////////////////////////////////////////////////////////////

        ///create padded buffer
        m_blurbuffer = new int[width + w -1];
        int* leftside = m_blurbuffer + ((w - 1) >>1);
        int* bufferend = m_blurbuffer + width + w -1;

        int * sadd;//will point to pixel to be added to the sum
        int * ssub;//will point to pixel to be subtracted from the sum
        int* b;//will point to buffer locations
        int sum;//will hold the sum of the neighborhood pixels

        ////for each row in input image
        while(p1 < endd)
        {
            //b is the buffer pointer
            /////create left pad
            b = m_blurbuffer;

            while(b < leftside)
            {
                *b = *p1;
                ++b;
            }


            /////copy image row to buffer
            rowend = p1 + width;
            while(p1 < rowend)
            {
                *b = *p1;
                b++;
                p1++;
            }

            /////create right pad
            p1--;//set pointer back to right end of row
            while(b < bufferend)
            {
                *b = *p1;
                b++;
            }


            /////copy blured pixels to tmpImg
            sum=0;



            //add fisrt neighborhood
            ssub = m_blurbuffer;
            sadd = m_blurbuffer;

            for(;sadd < w + m_blurbuffer; sadd++){

                sum += *sadd;//
            }

            *tmpImg = sum;//blur first fixel of the row
            tmpImg++;

            //copy averaged pixel values to tmpImg
            for(;sadd < bufferend; ++sadd)//
            {
                sum = sum - *ssub + *sadd;
                *tmpImg = sum;
                ssub++;//next element in the buffer
                tmpImg++;//next pixel
            }

            p1++;//go to the next row
        }

        delete [] m_blurbuffer;
        //end blur rows/////////////////////////////////////////////////////////////////////////////

        tmpImg = tmpImgStart;//reset tmpImg pointer

        //blur columns////////////////////////////////////////////////////////////////////////

        ///create padded buffer
        m_blurbuffer = new int[height + h -1];
        leftside = m_blurbuffer + ((h - 1) >>1);
        bufferend = m_blurbuffer + height + h -1;


        ////for each column in output image
        tmpImgrowend = tmpImgStart + width;//rowend marks the end of the first row of the image channel
        tmpImgColptr = tmpImgStart;//tmpPtr points to the top of a column, in this case to the first column
        p3colptr = p3;
        while(tmpImgColptr < tmpImgrowend)
        {
            //b is the buffer pointer
            /////create left pad
            b = m_blurbuffer;
            tmpImg = tmpImgColptr;//put tmpImg at the beggining of the column
            while(b < leftside)
            {
                *b = *tmpImg;
                ++b;
            }


            /////copy image column to buffer

            while(tmpImg < tmpImgEnd)//when p3 is more than endd it is past the last element of the column.
            {
                *b = *tmpImg;
                b++;
                tmpImg+= width;//moves new image channel pointer p3 to next pixel in the current column
            }


            /////create right pad
            tmpImg-= width;//set channel pointer back to the bottom of the column
            while(b < bufferend)
            {
                *b = *tmpImg;
                b++;
            }


            /////copy blured pixels back to image
            tmpImg = tmpImgColptr;//reset pointer to begining of column to copy blured pixel from buffer to the column
            p3 = p3colptr;
            sum=0;

            //add fisrt neighborhood
            ssub = m_blurbuffer;
            sadd = m_blurbuffer;

            for(;sadd < h + m_blurbuffer; sadd++){

                sum += *sadd;//
            }

            //*tmpImg = sum;//blur first fixel of the row
            *p3= sum / neighborhood_size;
            p3+=width;//advance the
            tmpImg+=width;

            //copy averaged pixel values to image
            for(;sadd < bufferend; ++sadd)
            {
                sum = sum - *ssub + *sadd;
                //*tmpImg = sum;
                *p3 = sum / neighborhood_size;
                ssub++;//next element in the buffer
                tmpImg+=width;//next pixel
                p3+=width;
            }

            tmpImgColptr++;//go to the next column in the temp image
            p3colptr++;//got to the next column in the output image
        }

        delete [] m_blurbuffer;

        //end blur columns ////////////////////////////////////////////////////////////////////////


        tmpImg = tmpImgStart; //reset tmpImg pointer

    }




    delete [] tmpImg;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::changeFilterW:
//
// Slot to process change in filter width caused by moving the slider.
//
void
Blur::changeFilterW(int value)
{
	// set value of m_slider[0] and tie it to m_slider[1] if necessary
	for(int i=0; i<2; i++) {
		m_slider [i]->blockSignals(true);
		m_slider [i]->setValue    (value);
		m_slider [i]->blockSignals(false);
		m_spinBox[i]->blockSignals(true);
		m_spinBox[i]->setValue    (value);
		m_spinBox[i]->blockSignals(false);

		// don't tie slider values if lock checkbox is not checked
		if(m_checkBox->checkState() != Qt::Checked) break;
	}

	// apply filter to source image and display result
	g_mainWindowP->preview();
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::changeFilterH:
//
// Slot to process change in filter height caused by moving the slider.
//
void
Blur::changeFilterH(int value)
{
	// set value of m_slider[1] and tie it to m_slider[0] if necessary
	for(int i=1; i>=0; i--) {
		m_slider [i]->blockSignals(true);
		m_slider [i]->setValue    (value);
		m_slider [i]->blockSignals(false);
		m_spinBox[i]->blockSignals(true);
		m_spinBox[i]->setValue    (value);
		m_spinBox[i]->blockSignals(false);

		// don't tie slider values if lock checkbox is not checked
		if(m_checkBox->checkState() != Qt::Checked) break;
	}

	// apply filter to source image and display result
	g_mainWindowP->preview();
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::setLock:
//
// Slot to process state change of "Lock" checkbox.
// Set both sliders to same (min) value.
//
void
Blur::setLock(int state)
{
	if(state == Qt::Checked) {
		int val = MIN(m_slider[0]->value(), m_slider[1]->value());
		for(int i=0; i<2; i++) {
			m_slider[i]->blockSignals(true);
			m_slider[i]->setValue(val);
			m_slider[i]->blockSignals(false);
		}
	}

	// apply filter to source image and display result
	g_mainWindowP->preview();
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::setPasses:
//
// slot to set whether the blur filter will be single pass or 2-pass
void
Blur::setPasses(int state)
{
    if(state == Qt::Checked){//if checked
        m_nPasses = 1;
    }else{
        m_nPasses = 2;
    }

    // apply filter to source image and display result
    g_mainWindowP->preview();

}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::reset:
//
// Reset parameters.
//
void
Blur::reset()
{
	m_slider[0]->setValue(3);
	m_slider[1]->setValue(3);
	m_checkBox->setCheckState(Qt::Checked);

	// apply filter to source image and display result
	g_mainWindowP->preview();
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::initShader:
//
// init shader program and parameters.
//
void
Blur::initShader() 
{
	m_nPasses = 2;
	// initialize GL function resolution for current context
	initializeGLFunctions();

	UniformMap uniforms;

	// init uniform hash table based on uniform variable names and location IDs
	uniforms["u_Wsize"  ] = WSIZE;
	uniforms["u_Step"   ] = STEP;
	uniforms["u_Sampler"] = SAMPLER;

        QString v_name = ":/vshader_passthrough";
        QString f_name = ":/hw2/fshader_blur1";
        
#ifdef __APPLE__
        v_name += "_Mac";
        f_name += "_Mac"; 
#endif    

	// compile shader, bind attribute vars, link shader, and initialize uniform var table
	g_mainWindowP->glw()->initShader(m_program[PASS1], 
	                                 v_name + ".glsl", 
	                                 f_name + ".glsl",
					 uniforms,
					 m_uniform[PASS1]);
	uniforms.clear();


	uniforms["u_Hsize"  ] = HSIZE;
	uniforms["u_Step"   ] = STEP;
	uniforms["u_Sampler"] = SAMPLER;

        v_name = ":/vshader_passthrough";
        f_name = ":/hw2/fshader_blur2";
        
#ifdef __APPLE__
        v_name += "_Mac";
        f_name += "_Mac"; 
#endif    

	// compile shader, bind attribute vars, link shader, and initialize uniform var table
	g_mainWindowP->glw()->initShader(m_program[PASS2], 
	                                 v_name + ".glsl", 
	                                 f_name + ".glsl",
					 uniforms,
					 m_uniform[PASS2]);

    //////initialize single pass blur shader//////
    uniforms.clear();


    uniforms["u_Wsize"  ] = WSIZE;
    uniforms["u_Hsize"  ] = HSIZE;
    uniforms["u_StepX"   ] = STEPX;
    uniforms["u_StepY"   ] = STEPY;
    uniforms["u_Sampler"] = SAMPLER;

        v_name = ":/vshader_passthrough";
        f_name = ":/hw2/fshader_blur_singlepass";

#ifdef __APPLE__
        v_name += "_Mac";
        f_name += "_Mac";
#endif

    // compile shader, bind attribute vars, link shader, and initialize uniform var table
    g_mainWindowP->glw()->initShader(m_program[SINGLE_PASS],
                                     v_name + ".glsl",
                                     f_name + ".glsl",
                     uniforms,
                     m_uniform[SINGLE_PASS]);


	m_shaderFlag = true;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Blur::gpuProgram:
//
// Active Blur gpu program
//
void
Blur::gpuProgram(int pass) 
{
	int w_size = m_slider[0]->value();
	int h_size = m_slider[1]->value();
	if(w_size % 2 == 0) ++w_size;
	if(h_size % 2 == 0) ++h_size;

    if(m_nPasses == 1){//if using the single pass filter
        glUseProgram(m_program[SINGLE_PASS].programId());
        glUniform1i (m_uniform[SINGLE_PASS][WSIZE], w_size);
        glUniform1i (m_uniform[SINGLE_PASS][HSIZE], h_size);
        glUniform1f (m_uniform[SINGLE_PASS][STEPX],  (GLfloat) 1.0f / m_width);
        glUniform1f (m_uniform[SINGLE_PASS][STEPY],  (GLfloat) 1.0f / m_width);
        glUniform1i (m_uniform[SINGLE_PASS][SAMPLER], 0);
        return;
    }

	switch(pass) {
		case PASS1:
			glUseProgram(m_program[PASS1].programId());
			glUniform1i (m_uniform[PASS1][WSIZE], w_size);
			glUniform1f (m_uniform[PASS1][STEP],  (GLfloat) 1.0f / m_width);
			glUniform1i (m_uniform[PASS1][SAMPLER], 0);
			break;
		case PASS2:
			glUseProgram(m_program[PASS2].programId());
			glUniform1i (m_uniform[PASS2][HSIZE], h_size);
			glUniform1f (m_uniform[PASS2][STEP],  (GLfloat) 1.0f / m_height);
			glUniform1i (m_uniform[PASS2][SAMPLER], 0);
			break;
	}
}
