//#define MINI_DATASET
#define SMALL_DATASET
//define MEDIUM_DATASET
//#define LARGE_DATASET
//#define EXTRALARGE_DATASET

#ifndef _SEIDEL_2D_H
	#define _SEIDEL_2D_H 
	# if !defined(MINI_DATASET) && !defined(SMALL_DATASET) && !defined(MEDIUM_DATASET) && !defined(LARGE_DATASET) && !defined(EXTRALARGE_DATASET)
		#define LARGE_DATASET 
	# endif

	# if !defined(TSTEPS) && !defined(N)
		# ifdef MINI_DATASET
			#define TSTEPS 20
			#define N 40
		# endif
		# ifdef SMALL_DATASET
			#define TSTEPS 40
			#define N 120
		# endif
		# ifdef MEDIUM_DATASET
			#define TSTEPS 100
			#define N 400
		# endif
		# ifdef LARGE_DATASET
			#define TSTEPS 500
			#define N 2000
		# endif
		# ifdef EXTRALARGE_DATASET
			#define TSTEPS 1000
			#define N 4000
		# endif
	#endif

	#include <stdio.h>
	#include <unistd.h>
	#include <string.h>
	#include <math.h>
	#include <stdlib.h>
	#include <math.h>
	#include <time.h>
	#include <sys/time.h>

#endif
