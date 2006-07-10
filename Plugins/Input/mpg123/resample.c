/* libmpgdec: An advanced MPEG layer 1/2/3 decoder.
 * resample.c: A dynamic resampler.
 *
 * Copyright (C) 2005-2006 William Pitcock <nenolod@nenolod.net>
 * Portions copyright (C) 1995-1999 Michael Hipp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mpg123.h"

#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > 32767.0) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < -32768.0) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = sum; }

#define NTOM_MUL (32768)
static unsigned long ntom_val[2] = { NTOM_MUL>>1,NTOM_MUL>>1 };
static unsigned long ntom_step = NTOM_MUL;


void mpgdec_synth_ntom_set_step(long m,long n)
{
	if(n >= 96000 || m >= 96000 || m == 0 || n == 0) {
		fprintf(stderr,"resampler: illegal rates\n");
		exit(1);
	}

	n *= NTOM_MUL;
	ntom_step = n / m;

	if(ntom_step > 8*NTOM_MUL) {
		fprintf(stderr,"max. 1:8 conversion allowed!\n");
		exit(1);
	}

	ntom_val[0] = ntom_val[1] = NTOM_MUL>>1;
}

int mpgdec_synth_ntom_8bit(mpgdec_real *bandPtr,int channel,unsigned char *samples,int *pnt)
{
  short samples_tmp[8*64];
  short *tmp1 = samples_tmp + channel;
  int i,ret;
  int pnt1 = 0;

  ret = mpgdec_synth_ntom(bandPtr,channel,(unsigned char *) samples_tmp,&pnt1);
  samples += channel + *pnt;

  for(i=0;i<(pnt1>>2);i++) {
    *samples = mpgdec_conv16to8[*tmp1>>AUSHIFT];
    samples += 2;
    tmp1 += 2;
  }
  *pnt += pnt1>>1;

  return ret;
}

int mpgdec_synth_ntom_8bit_mono(mpgdec_real *bandPtr,unsigned char *samples,int *pnt)
{
  short samples_tmp[8*64];
  short *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = mpgdec_synth_ntom(bandPtr,0,(unsigned char *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<(pnt1>>2);i++) {
    *samples++ = mpgdec_conv16to8[*tmp1>>AUSHIFT];
    tmp1 += 2;
  }
  *pnt += pnt1 >> 2;
  
  return ret;
}

int mpgdec_synth_ntom_8bit_mono2stereo(mpgdec_real *bandPtr,unsigned char *samples,int *pnt)
{
  short samples_tmp[8*64];
  short *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = mpgdec_synth_ntom(bandPtr,0,(unsigned char *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<(pnt1>>2);i++) {
    *samples++ = mpgdec_conv16to8[*tmp1>>AUSHIFT];
    *samples++ = mpgdec_conv16to8[*tmp1>>AUSHIFT];
    tmp1 += 2;
  }
  *pnt += pnt1 >> 1;

  return ret;
}

int mpgdec_synth_ntom_mono(mpgdec_real *bandPtr,unsigned char *samples,int *pnt)
{
  short samples_tmp[8*64];
  short *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = mpgdec_synth_ntom(bandPtr,0,(unsigned char *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<(pnt1>>2);i++) {
    *( (short *)samples) = *tmp1;
    samples += 2;
    tmp1 += 2;
  }
  *pnt += pnt1 >> 1;

  return ret;
}


int mpgdec_synth_ntom_mono2stereo(mpgdec_real *bandPtr,unsigned char *samples,int *pnt)
{
  int i,ret;
  int pnt1 = *pnt;

  ret = mpgdec_synth_ntom(bandPtr,0,samples,pnt);
  samples += pnt1;
  
  for(i=0;i<((*pnt-pnt1)>>2);i++) {
    ((short *)samples)[1] = ((short *)samples)[0];
    samples+=4;
  }

  return ret;
}


int mpgdec_synth_ntom(mpgdec_real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  static mpgdec_real buffs[2][2][0x110];
  static const int step = 2;
  static int bo = 1;
  short *samples = (short *) (out + *pnt);

  mpgdec_real *b0,(*buf)[0x110];
  int clip = 0; 
  int bo1;
  int ntom;

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = buffs[0];
    ntom = ntom_val[1] = ntom_val[0];
  }
  else {
    samples++;
    out += 2; /* to compute the right *pnt value */
    buf = buffs[1];
    ntom = ntom_val[1];
  }

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    mpgdec_dct64(buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = bo+1;
    mpgdec_dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
  }


  {
    register int j;
    mpgdec_real *window = mpgdec_decwin + 16 - bo1;
 
    for (j=16;j;j--,window+=0x10)
    {
      mpgdec_real sum;

      ntom += ntom_step;
      if(ntom < NTOM_MUL) {
        window += 16;
        b0 += 16;
        continue;
      }

      sum  = *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b0++;

      while(ntom >= NTOM_MUL) {
        WRITE_SAMPLE(samples,sum,clip);
        samples += step;
        ntom -= NTOM_MUL;
      }
    }

    ntom += ntom_step;
    if(ntom >= NTOM_MUL)
    {
      mpgdec_real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];

      while(ntom >= NTOM_MUL) {
        WRITE_SAMPLE(samples,sum,clip);
        samples += step;
        ntom -= NTOM_MUL;
      }
    }

    b0-=0x10,window-=0x20;
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x20,window-=0x10)
    {
      mpgdec_real sum;

      ntom += ntom_step;
      if(ntom < NTOM_MUL) {
        window -= 16;
        b0 += 16;
        continue;
      }

      sum = -*(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b0++;

      while(ntom >= NTOM_MUL) {
        WRITE_SAMPLE(samples,sum,clip);
        samples += step;
        ntom -= NTOM_MUL;
      }
    }
  }

  ntom_val[channel] = ntom;
  *pnt = ((unsigned char *) samples - out);

  return clip;
}

