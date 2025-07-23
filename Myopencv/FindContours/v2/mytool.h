#pragma once
#include "opencv2/opencv.hpp"
#include <core/types_c.h>
#include <core/core_c.h>
#include <imgproc/imgproc_c.h>
#include <vector>
/*
@datatime:2025-7-23
@version:2.0
@author:hong
@description:由于opencv版本的FindContours导致程序崩溃，根据官方案例进行修改
@development:qt5.4.1+visual studio 2013 + opencv4.4.0 + qmake
@note:

使用opencv并行加速
cv::parallel_for_(cv::Range(0, static_cast<int>(total)), [&](const cv::Range& range) {}
*/
 
using std::vector;
// 修改点1：添加inline关键字
inline void myFindContours(const cv::Mat& src,
	vector<vector<cv::Point>>& contours,
	vector<cv::Vec4i>& hierarchy,
	int retr = cv::RETR_LIST,
	int method = cv::CHAIN_APPROX_SIMPLE,
	cv::Point offset = cv::Point(0, 0));


/**
* @brief 查找图像中的轮廓并进行优化处理
* @param src 输入图像(8位单通道图像，非零像素视为1)
* @param[out] contours 输出的轮廓点集
* @param[out] hierarchy 输出的轮廓层级关系
* @param retr 轮廓检索模式(默认RETR_LIST)
* @param method 轮廓近似方法(默认CHAIN_APPROX_SIMPLE)
* @param offset 轮廓点偏移量(默认(0,0))
* @throw cv::Exception 如果输入图像无效或处理失败
* @note 此函数经过安全性和性能优化，线程安全
*/
inline void myFindContours(const cv::Mat& src,
	vector<vector<cv::Point>>& contours,
	vector<cv::Vec4i>& hierarchy,
	int retr,
	int method ,
	cv::Point offset )
{
	// 输入验证
	if (src.empty()) {
		contours.clear();
		hierarchy.clear();
		return;
	}

	if (src.type() != CV_8UC1) {
		CV_Error(cv::Error::StsBadArg, "输入图像必须是8位单通道");
	}

	// 清空输出
	contours.clear();
	hierarchy.clear();

	// 使用RAII管理资源
	cv::Ptr<CvMemStorage> storagePtr(cvCreateMemStorage(0), [](CvMemStorage* p) { cvReleaseMemStorage(&p); });
	CvSeq* _ccontours = nullptr;
 
	// 对于OpenCV 4.4.0使用clone副本+CvMat构造
	cv::Mat mutable_src = src.clone();
	CvMat c_image = cvMat(mutable_src.rows, mutable_src.cols, mutable_src.type(), mutable_src.data);
	c_image.step = static_cast<int>(mutable_src.step[0]);
	c_image.type = (c_image.type & ~cv::Mat::CONTINUOUS_FLAG) | (mutable_src.flags & cv::Mat::CONTINUOUS_FLAG);
 

	// 查找轮廓
	cvFindContours(&c_image, storagePtr, &_ccontours, sizeof(CvContour), retr, method, cvPoint(offset.x, offset.y));

	if (!_ccontours) {
		return; // 没有找到轮廓
	}

	// 获取所有轮廓序列
	cv::Seq<CvSeq*> all_contours(cvTreeToNodeSeq(_ccontours, sizeof(CvSeq), storagePtr));
	const size_t total = all_contours.size();
	contours.resize(total);
	hierarchy.resize(total);

	// 并行处理轮廓点
	cv::parallel_for_(cv::Range(0, static_cast<int>(total)), [&](const cv::Range& range) {
		for (int idx = range.start; idx < range.end; ++idx) {
			CvSeq* c = all_contours[idx];
			const int count = c->total;
			if (count <= 0) continue;

			// 直接拷贝到预分配的内存
			contours[idx].resize(count);
			cvCvtSeqToArray(c, contours[idx].data(), CV_WHOLE_SEQ);
		}
	});

	// 并行处理层级关系
	cv::parallel_for_(cv::Range(0, static_cast<int>(total)), [&](const cv::Range& range) {
		for (int idx = range.start; idx < range.end; ++idx) {
			CvSeq* c = all_contours[idx];
			hierarchy[idx] = cv::Vec4i(
				c->h_next ? CV_GET_SEQ_ELEM(CvContour, c->h_next, 0)->color : -1,
				c->h_prev ? CV_GET_SEQ_ELEM(CvContour, c->h_prev, 0)->color : -1,
				c->v_next ? CV_GET_SEQ_ELEM(CvContour, c->v_next, 0)->color : -1,
				c->v_prev ? CV_GET_SEQ_ELEM(CvContour, c->v_prev, 0)->color : -1
				);
		}
	});
}