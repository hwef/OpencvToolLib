#pragma once
#include <core/types_c.h>
#include <core/core_c.h>

// 修改点1：添加必要的OpenCV模块引用
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/sse_utils.hpp> // 用于SIMD优化
#include <vector>

/*
@datatime:2025-7-23
@version:1.0
@author:hong
@description:由于opencv版本的FindContours导致程序崩溃，根据官方案例进行修改
@development:qt5.4.1+visual studio 2013 + opencv4.4.0 + qmake
@note:

	原始版本网上找的
*/
using std::vector;
// 修改点1：添加inline关键字
inline void myFindContours(const cv::Mat& src,
	vector<vector<cv::Point>>& contours,
	vector<cv::Vec4i>& hierarchy,
	int retr = cv::RETR_LIST,
	int method = cv::CHAIN_APPROX_SIMPLE,
	cv::Point offset = cv::Point(0, 0));
 
//
// 注意：此函数签名和实现源自您提供的原始代码
inline void myFindContours(const cv::Mat& src,
	vector<vector<cv::Point>>& contours,
	vector<cv::Vec4i>& hierarchy,
	int retr ,
	int method ,
	cv::Point offset )
{
	contours.clear(); // 清空输出
	hierarchy.clear();
	// 根据OpenCV版本处理CvMat，您提供的代码片段如下：
#if CV_VERSION_REVISION &lt;= 6 // 注意：CV_VERSION_REVISION 是较老版本OpenCV的宏
	CvMat c_image = src; // 在旧版本中，cv::Mat可以直接转换为CvMat
	// 但请注意，cvFindContours可能会修改图像，所以最好使用副本
	// CvMat c_image = src.clone(); 这样更安全
#else
	// 对于较新的OpenCV版本 (3.x, 4.x)
	cv::Mat mutable_src = src.clone(); // cvFindContours会修改输入图像，务必使用副本
	CvMat c_image = cvMat(mutable_src.rows, mutable_src.cols, mutable_src.type(), mutable_src.data);
	c_image.step = static_cast<int>(mutable_src.step[0]); // 显式转换size_t到int
	c_image.type = (c_image.type & ~cv::Mat::CONTINUOUS_FLAG) | (mutable_src.flags & cv::Mat::CONTINUOUS_FLAG);
#endif
	cv::MemStorage storage(cvCreateMemStorage(0)); // 创建内存存储区
	CvSeq* _ccontours = nullptr; // C风格的轮廓序列指针
	// 根据OpenCV版本调用cvFindContours，您提供的代码片段如下：
#if CV_VERSION_REVISION <= 6
	cvFindContours(&c_image, storage, &_ccontours, sizeof(CvContour), retr, method, cvPoint(offset.x, offset.y));
#else
	cvFindContours(&c_image, storage, &_ccontours, sizeof(CvContour), retr, method, cvPoint(offset.x, offset.y)); // CvPoint构造方式一致
#endif
	if (!_ccontours) // 如果没有找到轮廓
	{
		contours.clear(); // 再次确保清空
		hierarchy.clear();
		// storage 会在 cv::MemStorage 对象析构时自动释放
		return;
	}
	// 使用 cvTreeToNodeSeq 获取所有轮廓的扁平序列，这对于后续处理（尤其是层级结构）更方便
	cv::Seq<CvSeq*> all_contours(cvTreeToNodeSeq(_ccontours, sizeof(CvSeq), storage));
	size_t total = all_contours.size();
	contours.resize(total); // 为轮廓数据预分配空间
	hierarchy.resize(total); // 为层级数据预分配空间
	cv::SeqIterator<CvSeq*> it = all_contours.begin();
	for (size_t i = 0; i < total; ++i, ++it)
	{
		CvSeq* c = *it;
		// 将轮廓的颜色（CvContour的成员）设置为其索引，用于后续层级信息的链接
		reinterpret_cast<CvContour*>(c)->color = static_cast<int>(i);
		int count = c->total; // 当前轮廓包含的点数
		if (count > 0) {
			// 您提供的原始代码中使用 new int[] 来中转点坐标
			int* data = new int[static_cast<size_t>(count * 2)]; // 分配临时内存存储x,y坐标对
			cvCvtSeqToArray(c, data, CV_WHOLE_SEQ); // 将CvSeq中的点集数据拷贝到data数组
			contours[i].reserve(count); // 为当前轮廓的点集预分配空间
			for (int j = 0; j < count; ++j) {
				contours[i].push_back(cv::Point(data[j * 2], data[j * 2 + 1]));
			}
			delete[] data; // 释放临时内存
		}
	}
	// 填充层级信息 (hierarchy)
	it = all_contours.begin(); // 重置迭代器
	for (size_t i = 0; i < total; ++i, ++it)
	{
		CvSeq* c = *it;
		// 通过之前设置的 color (即索引) 来获取层级关系
		int h_next = c->h_next ? reinterpret_cast<CvContour*>(c->h_next)->color : -1;
		int h_prev = c->h_prev ? reinterpret_cast<CvContour*>(c->h_prev)->color : -1;
		int v_next = c->v_next ? reinterpret_cast<CvContour*>(c->v_next)->color : -1; // 第一个子轮廓
		int v_prev = c->v_prev ? reinterpret_cast<CvContour*>(c->v_prev)->color : -1; // 父轮廓
		hierarchy[i] = cv::Vec4i(h_next, h_prev, v_next, v_prev);
	}
	// storage 会在 cv::MemStorage 对象析构时自动释放，无需显式调用 cvReleaseMemStorage
}
