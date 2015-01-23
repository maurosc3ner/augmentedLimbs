#include "PulseDetector.h"


//
// PulseDetector
//
PulseDetector::PulseDetector()
{
	// Setup classifier
	faceClassifier;
	if (!faceClassifier.load("haarcascade_frontalface_alt.xml")) {
		cerr << "Unable to load face classifier.\n";
		exit(1);
	}
	
    // Create PulseData object
    

	// Start clock for timestamping
	_start = boost::chrono::system_clock::now();
}

/*
 * detect and estimate bpm per loop
 * 
 */
double PulseDetector::detect(cv::Mat& frameOriginal) {
	
	
	
	// Video processing loop
	bool monitoring = true;

	//cv::waitKey(50); 
	// Read frame
	cv::Mat frameGreyscale, daGrayFace , scaleddaGrayFace;
	
	// Get some sleep after every read
	// This is important specially or the frame rate will be too fast for the algorithm
	//cv::waitKey(50); 

	if (!frameOriginal.empty()) {
		cv::Scalar color(0, 0, 255, 0);
		// Convert image to gray scale and equalize
		cv::cvtColor(frameOriginal, frameGreyscale, CV_BGR2GRAY);
		cv::equalizeHist(frameGreyscale, frameGreyscale);
		// Detect faces
		vector<cv::Rect> faces;
		faceClassifier.detectMultiScale(frameGreyscale, faces, 1.1, 3,
			CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_SCALE_IMAGE,
			cv::Size(30, 30));

		// We have detected a face
		// Draw boxes around detected faces and their foreheads
		if (!faces.empty()) {
			cv::Rect fh;
			cv::Rect grabbedFace;
			for (unsigned int i=0; i < faces.size(); i++) {
				
				// draw a green rectangle around the face over the original image
				if(monitoring) {
					getForehead(faces[i], fh);
					// This is locks the forehead
					getForehead(faces[0], _forehead); // Save forehead of 1st captured face
					cv::rectangle(frameOriginal, faces[i], cv::Scalar(0, 255, 0, 0), 1, 8, 0);
					cv::putText(frameOriginal, "Rostro", cv::Point(faces[i].x, faces[i].y), CV_FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(0, 255, 0, 0));
					cv::rectangle(frameOriginal, fh, cv::Scalar(0, 255, 255, 0), 1, 8, 0);
					cv::putText(frameOriginal, "Frente", cv::Point(faces[i].x, fh.y), CV_FONT_HERSHEY_PLAIN, 1.2, cv::Scalar(0, 255, 255, 0));
					
					if(_forehead.area() != 0) {
						// Get image data for the forehead for BPM monitoring
						cv::Mat fhimg = frameOriginal(_forehead);
						cv::rectangle(frameOriginal, _forehead, cv::Scalar(0, 255, 255, 0), 1, 8, 0);
						
						gap = (MAX_SAMPLES - (_means.size()) ) / _fps;
						if( gap > 0){
							char buffer[50];
							
							int n = snprintf(buffer, 50, "Calibrando %0.0lf s", gap);
							cv::putText(frameOriginal, buffer, cv::Point(_forehead.x + 100, _forehead.y+ 25), CV_FONT_HERSHEY_PLAIN, 1.2, color);
						}
						pdata = estimateBPM(fhimg);
						char bpmbuffer[50];
						int n = sprintf(bpmbuffer, "Estimado: %0.1lf bpm", (pdata.bpm));
						cv::putText(frameOriginal, bpmbuffer, cv::Point(_forehead.x+100 , _forehead.y), CV_FONT_HERSHEY_PLAIN, 1.2, color);						
					}
				}

			}//rof

		}//fi
		else
		{
			cout<<"rostro no detectado"<<endl;
			}
	}
	return pdata.bpm;
}

void PulseDetector::getForehead(const cv::Rect& face, cv::Rect& forehead) {
	getSubface(face, forehead, 0.50, 0.18, 0.25, 0.15);
	//getSubface(face, forehead, 0.50, 0.13, (0.45), 0.20);
	return;
}

void PulseDetector::getSubface(const cv::Rect& face, cv::Rect& sub, float sf_x, float sf_y, float sf_w, float sf_h) {
	assert (face.height != 0 && face.width != 0);
	assert (sf_w > 0.0 && sf_y > 0.0 && sf_w > 0.0 && sf_h > 0.0);
	sub.x = face.x + face.width * sf_x - (face.width * sf_w / 2.0);
	sub.y = face.y + face.height * sf_y - (face.height * sf_h / 2.0);
	sub.width = face.width * sf_w;
	sub.height = face.height * sf_h;
	return;
}
//
// Mathematical processing - Done without separating
// the frames as opposed to the original implementation
//
double PulseDetector::calculate_mean(const cv::Mat& image) {
	cv::Scalar means = cv::mean(image);
	return (means.val[0] + means.val[1] + means.val[2]) / 3;
}
//
// Main processing function
//
PU PulseDetector::estimateBPM(const cv::Mat& skin) {
	_means.push_back(calculate_mean(skin));
	_times.push_back(timestamp());

	PU pdata;
	int sampleSize = _means.size();
	// Check Point
	assert (_times.size() == sampleSize);

	// If there are no efficient samples, dont proceed
	if (sampleSize <= MIN_SAMPLES) {
		return pdata;
	}
	// If there are more samples than required, trim oldest
	if (sampleSize > MAX_SAMPLES) {
		list_trimfront(_means, MAX_SAMPLES);
		list_trimfront(_times, MAX_SAMPLES);
		list_trimfront(_bpms, MAX_SAMPLES);
		sampleSize = MAX_SAMPLES;
	}
	// FPS
	_fps = sampleSize / (_times.back() - _times.front());
	vector<double> even_times = linspace(_times.front(), _times.back(), sampleSize); 
	vector<double> interpolated = interp(even_times, _times, _means);

	//dump("interpolated" , interpolated);

	vector<double> hamming = hammingWindow(sampleSize);

	list_multiply_vector(interpolated, hamming);

	double totalMean = list_mean(interpolated);
	list_subtract(interpolated, totalMean);

	// One dimensional Discrete FFT
	vector<gsl_complex> fftraw = fft_transform(interpolated);
	//dump_complex(fftraw);

	vector<double> angles = calculate_complex_angle(fftraw);
	//dump("angles" , angles);
	
	// Get absolute values of FFT coefficients
	_fftabs = calculate_complex_abs(fftraw);

	// Frequencies using spaced values within interval 0 - L/2+1
	_frequencies = arange((sampleSize / 2) + 1);
	list_multiply(_frequencies, _fps / sampleSize);

	// Get indices of frequences that are less than 50 and greater than 150
	vector<double> freqs(_frequencies);
	list_multiply(freqs, 60.0);
	// Filter out frequencies less than 50 and greater than 180
	vector<double> fitered_indices = list_filter(freqs, BPM_FILTER_LOW, BPM_FILTER_HIGH);

	
	// Used filtered indices to get corresponding fft values, angles, and frequencies
	_fftabs = list_pruned(_fftabs, fitered_indices);
//	dump("Pruned FFT abs: ", _fftabs);
	freqs = list_pruned(freqs, fitered_indices);
//	dump("Pruned frequencies : ", freqs);
	angles = list_pruned(angles, fitered_indices);
	//dump("Pruned angles ", angles);	
	
	int max = list_argmax(_fftabs);

	_bpm = freqs[max];
	_bpms.push_back(_bpm);

	pdata.bpm = _bpm;

	return pdata;

}

void PulseDetector::clearBuffers()
{
	_means.clear();
	_times.clear();
	_fftabs.clear();
	_frequencies.clear();
	_pruned.clear();
	_prunedfreq.clear();
	_bpms.clear();
	gap = 0;
}

double PulseDetector::timestamp() {
	boost::chrono::duration<double> seconds = boost::chrono::system_clock::now() - _start;
	return seconds.count();
}
//
// Return a Hamming window
//http://docs.scipy.org/doc/numpy/reference/generated/numpy.hamming.html
//
vector<double> PulseDetector::hammingWindow(int M) {
	vector<double> window(M);
	if (M == 1) {
		window[0] = 1.0;
	} else {
		for (int n = 0; n < M; ++n) {
			window[n] = 0.54 - 0.46 * cos((2 * M_PI * n) / (M - 1));
		}
	}
	return window;
}

//
// Transform data to FFT  
//http://www.gnu.org/software/gsl/manual/gsl-ref_16.html
//
vector<gsl_complex> PulseDetector::fft_transform(vector<double>& samples) {
	int size = samples.size();
	double data[size];
	copy(samples.begin(), samples.end(), data);
	// Transform to fft
	gsl_fft_real_workspace* work = gsl_fft_real_workspace_alloc(size);
	gsl_fft_real_wavetable* real = gsl_fft_real_wavetable_alloc(size);
	gsl_fft_real_transform(data, 1, size, real, work);
	gsl_fft_real_wavetable_free(real);
	gsl_fft_real_workspace_free(work);
	// Unpack complex numbers
	gsl_complex unpacked[size];
	gsl_fft_halfcomplex_unpack(data, (double *) unpacked, 1, size);
	// Copy to  a vector
	int unpacked_size = size / 2 + 1;
	vector<gsl_complex> output(unpacked, unpacked + unpacked_size);
	return output;
}

//
// Get angles of raw fft coefficients
//
vector<double> PulseDetector::calculate_complex_angle(vector<gsl_complex> cvalues) {
	// Get angles for a given complex number
	vector<double> output(cvalues.size());
	for (unsigned int i = 0; i< cvalues.size(); i++) {
		double angle = atan2(GSL_IMAG(cvalues[i]), GSL_REAL(cvalues[i]));
		output[i] = angle;
	}
	return output;
}

vector<double> PulseDetector::calculate_complex_abs(vector<gsl_complex> cvalues) {
	// Calculate absolute value of a given complex number
	vector<double> output(cvalues.size());
	for (unsigned int i =0; i < cvalues.size(); i++) {
		output[i] = gsl_complex_abs(cvalues[i]);
	}
	return output;
}

//
// Interpolate function
//
vector<double> PulseDetector::interp(vector<double> interp_x, vector<double> data_x, vector<double> data_y) {
	assert (data_x.size() == data_y.size());
	vector<double> interp_y(interp_x.size());
	vector<double> interpRes;

	// GSL function expects an array
	double data_y_array[data_y.size()];
	double data_x_array[data_x.size()];
	copy (data_y.begin(), data_y.end(), data_y_array);
	copy (data_x.begin(), data_x.end(), data_x_array);

	double yi;
	int L = interp_x.size();

	gsl_interp_accel *acc = gsl_interp_accel_alloc ();
	gsl_spline *spline = gsl_spline_alloc (gsl_interp_linear, L);
	gsl_spline_init (spline, data_x_array, data_y_array, L);

	// BUGFIX: Need to iterate throuh given x-interpolation range
	for(unsigned int xi = 0; xi < interp_x.size(); xi++)
	{
		yi = gsl_spline_eval (spline, interp_x[xi], acc);
		interpRes.push_back(yi);
		//printf ("%g\n", yi);
	}

	gsl_spline_free (spline);
	gsl_interp_accel_free (acc);

	return interpRes;
}

vector<double> PulseDetector::arange(int stop) {
    vector<double> range(stop);
     for (int i=0; i < stop; i++) {
        range[i] = i;
    }
        return range;
}

//
// List operations
//

vector<double> PulseDetector::linspace(double start, double end, int count) {
	vector<double> intervals(count);
	double gap = (end - start) / (count - 1);
	intervals[0] = start;
	for (unsigned int i = 1; i < (count - 1); ++i) {
		intervals[i] = intervals[i-1] + gap;
	}
	intervals[count - 1] = end;
	return intervals;
}

double PulseDetector::list_mean(vector<double>& data) {
	assert (!data.empty());
	boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::mean> > acc;
	for_each(data.begin(), data.end(), boost::bind<void>(boost::ref(acc), _1));
	return boost::accumulators::mean(acc);
}

void PulseDetector::list_trimfront(vector<double>& list, int limit) {
	int excess = list.size() - limit;
	if (excess > 0) {
		list.erase(list.begin(), list.begin() + excess);
	}
}

void PulseDetector::list_subtract(vector<double>& data, double value) {
	for (unsigned int i = 0; i < data.size(); ++i) {
		data[i] -= value;
	}
}

void PulseDetector::list_multiply(vector<double>& data, double value) {
	for (unsigned int i = 0; i < data.size(); ++i) {
		data[i] *= value;
	}
}

void PulseDetector::list_multiply_vector(vector<double>& data, vector<double>& mult) {
	assert (data.size() == mult.size());
	for (unsigned int i = 0; i < data.size(); ++i) {
		data[i] *= mult[i];
	}
}

vector<double> PulseDetector::list_filter(vector<double>& data, double low, double high) {
	vector<double> indices;
    for (unsigned int i = 0; i < data.size(); ++i) {
    	if (data[i] >= low && data[i] <= high) {
    		indices.push_back(i);
    	}
    }
	return indices;
}

vector<double> PulseDetector::list_pruned(vector<double>& data, vector<double>& indices) {
	vector<double> pruned;
	for (unsigned int i = 0; i < indices.size(); ++i) {
		assert (indices[i] >= 0 && indices[i] < data.size());
		pruned.push_back(data[indices[i]]);
	}
	return pruned;
}

int PulseDetector::list_argmax(vector<double>& data) {
	int indmax;
	double argmax = 0;
	for (unsigned int i = 0; i < data.size(); ++i) {
		if (data[i] > argmax) {
			argmax = data[i];
			indmax = i;
		}
	}
	return indmax;
}
