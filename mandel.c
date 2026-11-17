/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  ./mandel -x -0.749 -y 0.1353 -m 50 -w 15
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include "jpegrw.h"
#include <math.h>

#define FRAMES 300
#define SCALE_START 5
#define SCALE_END 0.0001
#define MAX_PATH 30

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max );
static void show_help();


int main( int argc, char *argv[] )
{

	// Defaults
	double x_center = 0;
	double y_center = 0;
	double x_scale = 0;
	double y_scale = 0;
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 100;
	int    workers = 10;

	// Parse commands
	char c;
	while((c = getopt(argc,argv,"w:x:y:s:W:H:m:h"))!=-1) {
		switch(c) 
		{
			case 'w':
				workers = atoi(optarg);
			case 'x':
				x_center = atof(optarg);
				break;
			case 'y':
				y_center = atof(optarg);
				break;
			case 's':
				x_scale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
			default:
				break;
		}
	}

	// Create pipes
	int p2c[2];
	int c2p[2];
	if (pipe(p2c) < 0 || pipe(c2p) < 0) {
		perror("Pipe failed\n");
		return -1;
	}

	// Distribute load
	int task = 0;
	for (int i = 0; i < workers; i++)
	{
		const int pid = fork();

		if (pid < 0)
		{
			printf("Failed to fork\n");
			exit(1);
		}

		// Catch the child process
		if (pid == 0)
		{
			// Close parent to child write
			close(p2c[1]);
			// Close child to parent read
			close(c2p[0]);

			const int worker_id = i;
			// Create raw image
			imgRawImage* img = initRawImage(image_width,image_height);

			while( read( p2c[0], &task, sizeof(task) ) == sizeof(task) )
			{
				task++;
				write( c2p[1], &task, sizeof task );

				char outfile[MAX_PATH];
				sprintf(outfile, "output/%04d.jpg", task);

				float progress = (float) task / (FRAMES - 1);

				x_scale = SCALE_START * pow(SCALE_END / SCALE_START, progress);
				y_scale = x_scale / image_width * image_height;

				// Display the configuration of the image.
				printf("mandel %d: x=%lf y=%lf x_scale=%lf y_scale=%1f max=%d outfile=%s\n",task, x_center,y_center,x_scale,y_scale,max,outfile);

				// Fill image with black
				setImageCOLOR(img,0);

				// Compute the Mandelbrot image
				compute_image(img, x_center-x_scale/2, x_center+x_scale/2, y_center-y_scale/2, y_center+y_scale/2, max);

				// Save the image in the stated file.
				storeJpegImageFile(img, outfile);
			}

			freeRawImage(img);

			// Close parent to child read
			close(p2c[0]);
			// Close child to parent write
			close(c2p[1]);

			// Exit child process to stop loop
			printf("Worker %d is done\n", worker_id);
			return 0;
		}
	}

	// Close parent to child read
	close(p2c[0]);
	// Close child to parent write
	close(c2p[1]);

	do {
		write( p2c[1], &task, sizeof(task) );
	} while( read( c2p[0], &task, sizeof(task) ) == sizeof(task) && task < FRAMES );

	// Close parent to child write
	close(p2c[1]);
	// Close child to parent read
	close(c2p[0]);

	for ( int i = 0; i < WORKERS; i++ )
	{
		wait( NULL );
	}

	return 0;

}




/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max )
{
	int i,j;

	int width = img->width;
	int height = img->height;

	// For every pixel in the image...

	for(j=0;j<height;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(img,i,j,iteration_to_color(iters,max));
		}
	}
}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.jpg)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
