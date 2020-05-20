#include <iostream>
#include <fstream>
#include <sstream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include "include/ocv_versioninfo.h"
#include <math.h>
#include <map>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

/* 
	parameters for crop image and extract only the left dial.
	I have seen that these 3 possible crops give the best results
	for the hough transformation and in terms of performance.
 */
constexpr unsigned crop_start_x = 180;//160;//180;
constexpr unsigned crop_start_y = 250;//250;//300;
constexpr unsigned crop_width = 240;//290;//240;
constexpr unsigned crop_height = 250;//260;//150;
/* Empirical coefficient computed considering multiple experiments*/
constexpr double km_over_theta = 168.0/(180.0-63.0);
/* step of increasing theta: the smaller it is the best is the fit of the line */
constexpr unsigned theta_step = 1;



/* Given the frame of the left dial apply hough transformation to it and return the pair
	<rho, theta> which represents the most voted pair of parameters. 
	The difference from hough_transform function is that here I'm considering not the maximum
	voted pair but a mean of the two best.
*/
std::pair<double,double> hough_transform_mean(cv::Mat ldial){
	unsigned PI = 180;
	cv::Mat detected_edges;
	unsigned edgeThresh = 1;
	double lowThreshold = 1;
	unsigned ratio = 3;
	cv::imshow("dial",ldial);
	/* Find edges using Canny algorithm*/
	cv::Canny(ldial, detected_edges, lowThreshold, lowThreshold*ratio);

	/* Create accumulator: it is a linearized version of a matrix*/
	unsigned height = ldial.rows, width = ldial.cols;
	/* the maximum distance is given by the eucledian distance from angle to angle of the image*/
	unsigned diag_len = std::ceil(sqrt(width * width + height * height));
	unsigned num_thetas = 180;
	std::vector<double> accumulator(2 * diag_len*num_thetas,0);
	
	/* get the list of white points */
	std::vector<cv::Point> pnts;
	cv::findNonZero(detected_edges, pnts);
	
	double rmax = 0, thetamax = 0, rmax2 = 0, thetamax2 = 0;
	unsigned max_votes = 0, max_votes2 = 0;
	for(auto p: pnts){
		for(unsigned theta = 0; theta < PI; theta+= theta_step){
			double rho = std::round(p.x * cos(theta*M_PI/180.0) + p.y*sin(theta*M_PI/180.0));
			std::pair<double, double> p = std::make_pair(rho,theta*M_PI/180.0);
			accumulator[(rho + diag_len)*num_thetas + theta]+= 1;
			if(accumulator[(rho + diag_len)*num_thetas + theta] > max_votes){
				rmax = rho;
				max_votes = accumulator[(rho + diag_len)*num_thetas + theta];
				thetamax = theta*M_PI/180.0;
			}
			if(accumulator[(rho + diag_len)*num_thetas + theta] > max_votes2 && rmax != rho && thetamax != theta){
				rmax2 = rho;
				max_votes2 = accumulator[(rho + diag_len)*num_thetas + theta];
				thetamax2 = theta*M_PI/180.0;
			}
		}
	}
	/*Mean of the two maximum lines*/
	return std::make_pair((rmax+rmax2)/2.0, (thetamax+thetamax2)/2.0);
}


/* Given the frame of the left dial apply hough transformation to it and return the pair
	<rho, theta> which represents the most voted pair.
*/
std::pair<double,double> hough_transform(cv::Mat ldial){
	unsigned PI = 180;
	cv::Mat detected_edges;
	/* parameters for canny function */
	unsigned edgeThresh = 1;
	unsigned kernel_size = 3;
	double lowThreshold = 1;
	unsigned ratio = 3;
	/* find edjes using Canny. The idea is to reduce as much as possible the number of white pixels.*/
	cv::Canny(ldial, detected_edges, lowThreshold, lowThreshold*ratio);

	unsigned height = ldial.rows, width = ldial.cols;
	/* The maximum distance of rho is the eucledian distance from angle to angle of the frame */
	unsigned diag_len = std::ceil(sqrt(width * width + height * height));
	unsigned num_thetas = 180;
	/* accumulator is a linearized matrix */
	std::vector<double> accumulator(2 * diag_len*num_thetas,0);
	
	double rmax = 0, thetamax = 0;
	unsigned max_votes = 0;
	for(unsigned x = 0; x < detected_edges.rows; ++x){
		for(unsigned y = 0; y < detected_edges.cols; ++y){
			unsigned col = detected_edges.at<uchar>(x,y);
			if(col == 255){
				for(unsigned theta = 0; theta < PI; theta+= theta_step){
					double rho = std::round(y * cos(theta*M_PI/180.0) + x*sin(theta*M_PI/180.0));
					std::pair<double, double> p = std::make_pair(rho,theta*M_PI/180.0);
					/* vote for parameters rho and theta */ 
					accumulator[(rho + diag_len)*num_thetas + theta]+= 1;
					/* if the number of votes is greater then the previous vote update */
					if(accumulator[(rho + diag_len)*num_thetas + theta] > max_votes){
						rmax = rho;
						max_votes = accumulator[(rho + diag_len)*num_thetas + theta];
						thetamax = theta*M_PI/180.0;
					}
				}
			}
		}
	}
	/* return max pair found with hough transform*/
	return std::make_pair(rmax, thetamax);
}


/**
	Debug mode plots the straight line founded with hough transformation
	into the original frame and add a new frame to the debug video.
**/
void debug_mode(double rho, double theta, cv::Mat frame, cv::VideoWriter debugVideo){

	cv::Point pt1, pt2;
	double a = cos(theta), b = sin(theta);
	double x0 = a*rho, y0 = b*rho;

	/* draw a red line with lenght 1000 that pass from point pt1 and pt2*/
	pt1.x = crop_start_x+cvRound(x0 + 1000*(-b));
	pt1.y = crop_start_y+cvRound(y0 + 1000*(a));
	pt2.x = crop_start_x+cvRound(x0 - 1000*(-b));
	pt2.y = crop_start_y+cvRound(y0 - 1000*(a));
	cv::line(frame, pt1, pt2, cv::Scalar(0,0,255), 3, CV_AA);
	
	cv::imshow("Line in original Frame", frame);
	debugVideo << frame;
}

/* Function which converts the angle into the speed */
double evalSpeed(double th, unsigned &side){
	double theta_deg = th*180.0/M_PI;
	double speed=0;
	/* 	direction can be side = 1 left or side = 0 rigth. 
		when the angle is greater then more or less 180 side is set to 0 */
	if (side == 1) /* if left */
		speed = (theta_deg-63)*km_over_theta;
	if (side == 0 && theta_deg < 170)/* if right */
		speed = (theta_deg+180-63)*km_over_theta;
	if(theta_deg >= 178)
		side = 0;
	return std::max(speed,0.0);
}


int main( int argc, char* argv[] ){
    print_ocv_version();
    cv::VideoCapture car("carspeed.mp4");
    unsigned side = 1; /* 1 left 0 right */
    unsigned frames = 0;

	std::ofstream outfile;
  	outfile.open ("speed.csv");
  	outfile << "speed,frame\n";

  	cv::Size S = cv::Size((int) car.get(CV_CAP_PROP_FRAME_WIDTH),    //Acquire input size
              (int) car.get(CV_CAP_PROP_FRAME_HEIGHT));
	cv::VideoWriter debugVideo;
	int ex = static_cast<int>(car.get(CV_CAP_PROP_FOURCC));
	debugVideo.open("output.mp4" , ex, car.get(CV_CAP_PROP_FPS),S, true);

    while(1){
 	
	    cv::Mat frame;
	    car >> frame;
	    if (frame.empty())
	      break;
	 
		cv::Rect crop(crop_start_x, crop_start_y, crop_width ,crop_height);//best		

	    cv::Mat left_dial(frame, crop);
	    cv::Mat Ibw_dial;
	    cv::Mat Ia;
        cv::cvtColor(left_dial,Ia, cv::COLOR_RGB2GRAY); // transform to a gray scale image. Igs output

		double thres_val = cv::threshold(Ia, Ibw_dial, 0, 255, CV_THRESH_OTSU);
	   	std::pair<double,double> linePar = hough_transform(Ibw_dial);
	   	/* plot debug mode */
		debug_mode(linePar.first, linePar.second, frame, debugVideo);
		/* compute speed from the angle */
		double speed = evalSpeed(linePar.second, side);

		frames+=1;
	    std::cout << "Time: "<< frames/30.0 <<"s \t angle: "<< linePar.second << "\t car speed: "<< speed<<std::endl;  
		std::cout << "================================================================"<< std::endl;

		if(speed > 0) outfile << ""<<speed<<","<< frames<<"\n";
	    char c=(char)cv::waitKey(25);
	    if(c=='q')
	    	break;
	}
	outfile << "\n";
	outfile.close();

    return 0;
}