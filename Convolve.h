// ======================================================================
// IMPROC: Image Processing Software Package
// Copyright (C) 2016 by George Wolberg
//
// Convolve.h - Convolve widget
//
// Written by: George Wolberg, 2016
// ======================================================================

#ifndef CONVOLVE_H
#define CONVOLVE_H

#include "ImageFilter.h"

class Convolve : public ImageFilter {
	Q_OBJECT

public:
	Convolve			(QWidget *parent = 0);	// constructor
	QGroupBox*	controlPanel	();			// create control panel
	bool		applyFilter	(ImagePtr, bool, ImagePtr);	// apply filter to input
	void		convolve	(ImagePtr, ImagePtr, ImagePtr);
	void		initShader();
	void		gpuProgram(int pass);	// use GPU program to apply filter

protected slots:
	int		load		();
    void     kernelMode  (int);

private:
	// widgets
	QPushButton*	m_button;	// Convolve pushbutton
	QTextEdit*	m_values;	// text field for kernel values
	QGroupBox*	m_ctrlGrp;	// groupbox for panel
    QCheckBox*  m_checkBox; //

	// variables
	QString		m_file;
	QString		m_currentDir;
	ImagePtr	m_kernel;
    GLuint      m_kernelTex;

    int		m_width;	   // input image width
    int		m_height;	   // input image height

    bool    m_usingarray;  // if true the uniform array based shader will be used
                           // otherwise the texture based shader will be used

};

#endif	// CONVOLVE_H
