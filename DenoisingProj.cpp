// DenoisingProj.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "windows.h"
#include <string>
#include <vector>
#include <iostream>
#include "opencv2/opencv.hpp"

// *** code for generating noisy images *** //
/* AddGaussianNoise() : add noise with Gaussian distribution
input arguments:
cv::Mat img: original input image, image to add noise to
float stdv: standard deviation of Gaussian noise distribution
float mean: mean of of Gaussian noise distribution
output return value:
cv::Mat ret: image with added noise
*/
cv::Mat AddGaussianNoise(cv::Mat img, float stdv = 5.f, float mean = 0.f)
{
	cv::Mat img_32, noise;
	if (img.channels() == 3) {
		noise = cv::Mat(cv::Size(img.rows, img.cols), CV_32FC3);
		img.convertTo(img_32, CV_32FC3);
	}
	else {
		noise = cv::Mat(cv::Size(img.rows, img.cols), CV_32F);
		img.convertTo(img_32, CV_32F);
	}
	cv::randn(noise, mean, stdv);
	cv::Mat noisy_img = img_32 + noise;
	noisy_img.convertTo(noisy_img, CV_8UC3);
	return noisy_img;
}

/* MakeNoisyImageDB() : add Gaussian noise for all images in DB
input arguments:
std::vector<cv::Mat> &orig_imgs: array of original input images
float s, float m: standard deviation and mean of Gaussian distribution
output return value:
std::vector<cv::Mat> &noisy_imgs: array of noisy output images
*/
void MakeNoisyImageDB(std::vector<cv::Mat> &orig_imgs,
	std::vector<cv::Mat> &noisy_imgs, float s = 5.f, float m = 0.f)
{
	noisy_imgs.clear();
	int nimg = orig_imgs.size();
	for (int i = 0; i < nimg; i++) {
		noisy_imgs.push_back(AddGaussianNoise(orig_imgs[i], s, m));
	}
}
// *** END of code for generating noisy images *** //


// *** code for loading db *** //
// make list of filenames in directory d
void getDirFilenameList(const wchar_t* d,
	std::vector<std::string> & f)
{
	//std::vector<std::string> names;
	f.clear();
	std::wstring search_path = std::wstring(d) + _T("/*.*");
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			// read all (real) files in current folder
			// , delete '!' read other 2 default folder . and ..
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				char str[260];
				std::wstring fn = fd.cFileName;
				std::wstring ext = fn.substr(fn.find_last_of(_T(".")) + 1);
				if (ext == _T("tiff") || ext == _T("png")) {
					wcstombs_s(NULL, str, fn.length() + 1, fn.c_str(), 260);
					f.push_back(str);
				}
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}

	/*
	FILE* pipe = NULL;
	std::wstring pCmd = _T("dir /B /S ") + std::wstring(d);
	char buf[256];

	if (NULL == (pipe = _wpopen(pCmd.c_str(), _T("rt")))) {
	std::cout << "no such directory" << std::endl;
	return;
	}

	while (!feof(pipe)) {
	if (fgets(buf, 256, pipe) != NULL)
	{
	f.push_back(std::string(buf));
	}
	}
	_pclose(pipe);
	*/
}

// load all images in directory dir_path
void LoadImageDB(const wchar_t* dir_path,
	std::vector<cv::Mat> &img_arr)
{
	// store current directory	
	wchar_t* cdir = _wgetcwd(NULL, 0);
	// change working directory to input
	_wchdir(dir_path);

	// get filename list
	std::vector<std::string> filenames;
	getDirFilenameList(dir_path, filenames);
	// load all images in filename list
	int nimgs = filenames.size();
	img_arr.clear();
	for (int i = 0; i < nimgs; i++) {
		img_arr.push_back(cv::imread(filenames[i]));
	}
	// restore current directory
	_wchdir(cdir);
}

void SaveImageDB(const wchar_t* dir_path,
	std::vector<std::string> &filenames,
	std::vector<cv::Mat> &img_arr)
{
	// if filenames are inconsistent with images, do nothing
	if (filenames.size() != img_arr.size()) {
		return;
	}
	// store current directory	
	wchar_t* cdir = _wgetcwd(NULL, 0);
	// change working directory path
	_wchdir(dir_path);
	// save all images with filenames
	int fns = filenames.size();
	for (int i = 0; i < fns; i++) {
		cv::imwrite(filenames[i], img_arr[i]);
	}
	// restore current directory
	_wchdir(cdir);
}

void SaveNoisyImageDB(std::vector<cv::Mat> &noisy_imgs,
	const wchar_t* orig_dir,
	const wchar_t* noisy_dir)
{
	// get filename list
	std::vector<std::string> filenames;
	getDirFilenameList(orig_dir, filenames);
	// save images in noisy_dir with filenames
	SaveImageDB(noisy_dir, filenames, noisy_imgs);
}
// *** END of code for loading db *** //


/* *** code for computing denoising performance
performance measured in terms of similarity
of result and original no-noise image
*/
double ComputeMSE(cv::Mat img, cv::Mat ref)
{
	cv::Mat s1;
	cv::absdiff(img, ref, s1);      // |img - ref|
	if (s1.channels() == 3)
		s1.convertTo(s1, CV_32FC3);
	else
		s1.convertTo(s1, CV_32F);	// cannot make a square on 8 bits
	s1 = s1.mul(s1);				// |I1 - I2|^2
	cv::Scalar s = cv::sum(s1);
	return s.val[0] + s.val[1] + s.val[2];
}

double ComputePSNR(cv::Mat img, cv::Mat ref)
{
	double sse = ComputeMSE(img, ref); // sum channels

	if (sse <= 1e-10) // for small values return zero
		return 0;
	else
	{
		double mse = sse / (double)(img.channels() * img.total());
		double psnr = 10.0*log10((255 * 255) / mse);
		return psnr;
	}
}

double ComputePSNR_Avg(std::vector<cv::Mat> &denoised_imgs,
	std::vector<cv::Mat> &orig_imgs)
{
	// if number of denoised-original image pairs are inconsistent do nothing
	if (denoised_imgs.size() != orig_imgs.size()) {
		printf("number of denoised and original images are inconsistent\n");
		exit(1);
	}
	int nimg = denoised_imgs.size();
	double psnr_sum = 0;
	for (int i = 0; i < nimg; i++) {
		double psnr_img = ComputePSNR(denoised_imgs[i], orig_imgs[i]);
		psnr_sum += psnr_img;
	}
	return psnr_sum / (double)nimg;
}


int _tmain(int argc, _TCHAR* argv[])
{
	// *** declare objects for input and output image arrays *** //
	// array of original images
	std::vector<cv::Mat> orig_imgs;
	// array of noisy images
	std::vector<cv::Mat> noisy_imgs;

	// IMPORTANT: YOU SHOULD CHANGE DIRECTORY PATHS BELOW ACCORDING TO YOUR ENVIRONMENT
	const wchar_t* orig_dir = _T("C:\\Users\\user\\Desktop\\FinalProject\\data\\orig");
	const wchar_t* noisy_dir = _T("C:\\Users\\user\\Desktop\\FinalProject\\data\\noisy");
	const wchar_t* denoised_dir = _T("C:\\Users\\user\\Desktop\\FinalProject\\data\\denoised");
	// *** load original database images *** //
	LoadImageDB(orig_dir, orig_imgs);
	// *** generate & store noisy images - DONE *** // 
	//MakeNoisyImageDB(orig_imgs, noisy_imgs, 20);
	//SaveNoisyImageDB(noisy_imgs, orig_dir, noisy_dir);
	//noisy_imgs.clear();
	// *** load generated noisy database images *** //
	LoadImageDB(noisy_dir, noisy_imgs);

	// *** denoise noisy images *** //
	// array of denoised images
	std::vector<cv::Mat> denoised_imgs;
	// YOUR CODE HERE 
	// - run denoising on all images in orig_imgs array, 
	// - store in denoised_imgs array
	cv::Mat kernel5 = (cv::Mat_<char>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);

	int nimg = noisy_imgs.size();
	denoised_imgs.clear();
	for (int i = 0; i < nimg; i++) {
		cv::Mat now_nosiyimg = noisy_imgs[i].clone();
		cv::Mat denoised_img_save = noisy_imgs[i].clone();

		cv::bilateralFilter(now_nosiyimg, denoised_img_save, 5, 25, 25);
		cv::GaussianBlur(denoised_img_save, now_nosiyimg, cv::Size(5, 5), 0, 0);
		cv::filter2D(now_nosiyimg, denoised_img_save, now_nosiyimg.depth(), kernel5);
		cv::bilateralFilter(denoised_img_save, now_nosiyimg, 15, 30, 30);
		cv::bilateralFilter(now_nosiyimg, denoised_img_save, 5, 10, 10);
		cv::bilateralFilter(denoised_img_save, now_nosiyimg, 3, 6, 6);
		denoised_imgs.push_back(now_nosiyimg);
	}

	std::vector<std::string> filenames;
	getDirFilenameList(orig_dir, filenames);
	SaveImageDB(denoised_dir, filenames, denoised_imgs);

	// END OF YOUR CODE

	// *** compute quality of denoised images *** //
	double q = ComputePSNR_Avg(denoised_imgs, orig_imgs);
	//double q = ComputePSNR_Avg(noisy_imgs, orig_imgs);
	printf("PSNR of denoised image (higher is better): %lf\n", q);
	wchar_t buffer[260];
	swprintf(buffer, 260, _T("PSNR of denoised image (higher is better): %lf\n"), q);
	OutputDebugStringW(buffer);
	return 0;
}