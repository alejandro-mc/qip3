// ======================================================================
// IMPROC: Image Processing Software Package
// Copyright (C) 2016 by George Wolberg
//
// Correlation.h - Correlation widget
//
// Written by: George Wolberg, 2016
// Modified by: Alejandro Morejon Cortina, 2016
// ======================================================================

#ifndef CORRELATION_H
#define CORRELATION_H
#include "ImageFilter.h"

class Correlation : public ImageFilter {
	Q_OBJECT

public:
    Correlation			(QWidget *parent = 0);	// constructor
	QGroupBox*	controlPanel	();			// create control panel
	bool		applyFilter	(ImagePtr, bool, ImagePtr);	// apply filter to input
    void		correlation	(ImagePtr, ImagePtr);
	void		initShader();
	void		gpuProgram(int pass);	// use GPU program to apply filter

protected slots:
	int		load		();

private:
	// widgets
	QPushButton*	m_button;	// Convolve pushbutton
	QTextEdit*	m_values;	// text field for kernel values
	QGroupBox*	m_ctrlGrp;	// groupbox for panel

	// variables
	QString		m_file;
	QString		m_currentDir;
	ImagePtr	m_kernel;
    ImagePtr    m_kernelGray;
    GLuint      m_tmpltTex;
    GLuint      m_corrValsFBO;//id of the frame buffer that holds correlation values
    GLuint      m_corrValsText;//id of the correlation values texture
    QImage      m_tmpltImage;
    QImage      m_qIm;
    float*      m_corrValues;//holds the correlation values downloaded from GPU
    int		    m_width;	 // input image width
    int		    m_height;	 // input image height
};

#endif	// CONVOLVE_H
