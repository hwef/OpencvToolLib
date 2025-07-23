#pragma once
#include "opencv2/opencv.hpp"
#include <core/types_c.h>
#include <core/core_c.h>
#include <imgproc/imgproc_c.h>
#include <vector>
#include "opencv2/opencv.hpp"
#include <vector>
#include <omp.h>
#include <algorithm>  // for std::max

/*
@datatime:2025-7-23
@version:3.0
@author:hong
@description:由于opencv版本的FindContours导致程序崩溃，根据官方案例进行修改
@development:qt5.4.1+visual studio 2013 + opencv4.4.0 + qmake
@note:

	相较于v2版本，使用Openmp并行加速 替换了原始的cv:parallel_for_
	并设计了自动根据图像大小优化并行度

*/
 
using std::vector;
// 修改点1：添加inline关键字
inline void myFindContours(const cv::Mat& src,
	vector<vector<cv::Point>>& contours,
	vector<cv::Vec4i>& hierarchy,
	int retr = cv::RETR_LIST,
	int method = cv::CHAIN_APPROX_SIMPLE,
	cv::Point offset = cv::Point(0, 0));

using std::vector;

/**
* @brief 安全高效的轮廓查找函数（自动线程优化版）
* @param src 8位单通道二值图像
* @param[out] contours 输出轮廓点集
* @param[out] hierarchy 输出轮廓层级
* @param retr 轮廓检索模式
* @param method 轮廓近似方法
* @param offset 点坐标偏移量
* @note 自动根据图像大小优化并行度
*/
inline void myFindContours(const cv::Mat& src,
	vector<vector<cv::Point>>& contours,
	vector<cv::Vec4i>& hierarchy,
	int retr ,
	int method,
	cv::Point offset)
{
	// RAII守卫保护OMP设置
	struct OMPGuard {
		int original;
		OMPGuard() : original(omp_get_max_threads()) {}
		~OMPGuard() { omp_set_num_threads(original); }
	} omp_guard;

	// 自动优化并行度
	const int64_t pixel_count = static_cast<int64_t>(src.rows) * src.cols;
	const int64_t small_threshold = 512 * 512;    // 0.25MP
	const int64_t medium_threshold = 2048 * 2048; // 4MP

	int optimal_threads = omp_get_max_threads();
	if (pixel_count < small_threshold) {
		optimal_threads = 1;  // 小图像单线程
	}
	else if (pixel_count < medium_threshold) {
		optimal_threads = std::max(2, optimal_threads / 2);  // 中等图像半线程
	}
	omp_set_num_threads(optimal_threads);

	// 输入验证
	if (src.empty()) {
		contours.clear();
		hierarchy.clear();
		return;
	}

	if (src.type() != CV_8UC1) {
		CV_Error(cv::Error::StsBadArg, "输入图像必须是8位单通道");
	}

	// RAII资源管理
	struct ContourStorage {
		CvMemStorage* storage;
		ContourStorage() : storage(cvCreateMemStorage(0)) {}
		~ContourStorage() {
			if (storage) cvReleaseMemStorage(&storage);
		}
	} storageWrapper;

	CvSeq* firstContour = nullptr;
	cv::Mat mutable_src = src.clone();

	// 安全构造CvMat
	CvMat c_image;
	c_image.rows = mutable_src.rows;
	c_image.cols = mutable_src.cols;
	c_image.type = mutable_src.type();
	c_image.data.ptr = mutable_src.data;
	c_image.step = static_cast<int>(mutable_src.step[0]);

	// 查找轮廓
	cvFindContours(&c_image, storageWrapper.storage, &firstContour,
		sizeof(CvContour), retr, method, cvPoint(offset.x, offset.y));

	if (!firstContour) {
		contours.clear();
		hierarchy.clear();
		return;
	}

	// 轮廓序列转换
	CvSeq* seq = cvTreeToNodeSeq(firstContour, sizeof(CvSeq), storageWrapper.storage);
	const int total = seq->total;
	if (total == 0) {
		contours.clear();
		hierarchy.clear();
		return;
	}

	contours.resize(total);
	hierarchy.resize(total);

	// 并行处理轮廓
#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < total; ++i) {
		CvSeq* contour = (CvSeq*)cvGetSeqElem(seq, i);
		const int count = contour->total;

		if (count > 0) {
			contours[i].resize(count);
			cvCvtSeqToArray(contour, contours[i].data(), CV_WHOLE_SEQ);
		}

		// 层级关系处理
		hierarchy[i] = cv::Vec4i(
			contour->h_next ? ((CvContour*)contour->h_next)->color : -1,
			contour->h_prev ? ((CvContour*)contour->h_prev)->color : -1,
			contour->v_next ? ((CvContour*)contour->v_next)->color : -1,
			contour->v_prev ? ((CvContour*)contour->v_prev)->color : -1
			);
	}
}