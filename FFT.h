/******      FFT      ******/
#ifndef FFT_H_
#define FFT_H_

#include "main.h"						// Global includes and definitions
#include <arduinoFFT.h>					// FFT

#define N_FFT				512
#define AVG_VOLUME_ALPHA	0.95

extern double curr_volume, avg_volume;
extern double fft_real[N_FFT];
extern bool windowFFT;


/*********************************************/
/******      FFT related functions      ******/
/*********************************************/
void computeFFT(bool window);
void performFFT(unsigned int buf_id, bool apply_EMA = true);

#endif

