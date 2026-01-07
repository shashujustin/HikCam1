#include "HikCam.h"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

class GreenCircleDetector {
public:
    GreenCircleDetector() {
        // 初始化绿色范围（针对前哨站灯光优化）
        init_green_range();
        
        // 创建显示窗口
        cv::namedWindow("Camera View", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("Green Circle Detection", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("Green Mask", cv::WINDOW_NORMAL);
        cv::resizeWindow("Green Mask", 640, 480);
        
        std::cout << "前哨站绿色圆形灯识别系统初始化完成" << std::endl;
        std::cout << "==========================================" << std::endl;
        std::cout << "按 'ESC' 或 'q' 键退出程序" << std::endl;
        std::cout << "按 's' 键保存当前帧" << std::endl;
        std::cout << "按 'r' 键重置参数" << std::endl;
        std::cout << "按 '+' 键增加圆形度阈值" << std::endl;
        std::cout << "按 '-' 键降低圆形度阈值" << std::endl;
        std::cout << "==========================================" << std::endl;
    }
    
    ~GreenCircleDetector() {
        cv::destroyAllWindows();
    }
    
    // 处理每一帧图像
    void processFrame(const cv::Mat& frame) {
        if (frame.empty()) {
            return;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 检测绿色圆形
        cv::Mat result = detectGreenCircles(frame);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 显示处理时间
        if (!result.empty()) {
            cv::putText(result, 
                       "FPS: " + std::to_string(1000.0 / (duration.count() > 0 ? duration.count() : 1)),
                       cv::Point(10, 30), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
        }
        
        // 显示结果
        if (!result.empty()) {
            cv::imshow("Green Circle Detection", result);
        }
        cv::imshow("Camera View", frame);
    }
    
    // 处理按键输入
    void handleKeyPress(int key) {
        switch (key) {
            case 's':  // 保存图像
            case 'S':
                saveCurrentFrame();
                break;
            case 'r':  // 重置参数
            case 'R':
                resetParameters();
                break;
            case '+':  // 增加圆形度阈值
                circularity_threshold_ = std::min(circularity_threshold_ + 0.05, 1.0);
                std::cout << "圆形度阈值增加至: " << circularity_threshold_ << std::endl;
                break;
            case '-':  // 降低圆形度阈值
                circularity_threshold_ = std::max(circularity_threshold_ - 0.05, 0.1);
                std::cout << "圆形度阈值降低至: " << circularity_threshold_ << std::endl;
                break;
            case '1':  // 调整绿色范围下限H
                green_lower_[0] = std::max(green_lower_[0] - 5, 0.0);
                std::cout << "绿色H下限: " << green_lower_[0] << std::endl;
                break;
            case '2':  // 调整绿色范围上限H
                green_lower_[0] = std::min(green_lower_[0] + 5, 180.0);
                std::cout << "绿色H下限: " << green_lower_[0] << std::endl;
                break;
            case '3':  // 调整绿色范围下限S
                green_lower_[1] = std::max(green_lower_[1] - 10, 0.0);
                std::cout << "绿色S下限: " << green_lower_[1] << std::endl;
                break;
            case '4':  // 调整绿色范围上限S
                green_lower_[1] = std::min(green_lower_[1] + 10, 255.0);
                std::cout << "绿色S下限: " << green_lower_[1] << std::endl;
                break;
        }
    }

private:
    cv::Scalar green_lower_;
    cv::Scalar green_upper_;
    cv::Mat current_frame_;
    cv::Mat green_mask_;
    int frame_counter_ = 0;
    
    // 圆形检测参数
    double circularity_threshold_ = 0.7;  // 圆形度阈值（0-1，越接近1越接近圆）
    double min_area_ = 100.0;            // 最小面积
    double max_area_ = 5000.0;           // 最大面积
    double min_radius_ = 5.0;            // 最小半径
    double max_radius_ = 50.0;           // 最大半径
    
    void init_green_range() {
        // 针对前哨站绿色灯的优化范围
        // 可能需要根据实际灯光颜色调整
        green_lower_ = cv::Scalar(40, 100, 100);   // H: 40-80, S: 100-255, V: 50-255
        green_upper_ = cv::Scalar(80, 255, 255);
        
        print_parameters();
    }
    
    void print_parameters() {
        std::cout << "\n当前参数设置:" << std::endl;
        std::cout << "绿色HSV范围: H(" << green_lower_[0] << "-" << green_upper_[0] 
                  << ") S(" << green_lower_[1] << "-" << green_upper_[1] 
                  << ") V(" << green_lower_[2] << "-" << green_upper_[2] << ")" << std::endl;
        std::cout << "圆形度阈值: " << circularity_threshold_ << std::endl;
        std::cout << "面积范围: " << min_area_ << " - " << max_area_ << std::endl;
        std::cout << "半径范围: " << min_radius_ << " - " << max_radius_ << std::endl;
        std::cout << std::endl;
    }
    
    // 计算轮廓的圆形度
    double calculateCircularity(const std::vector<cv::Point>& contour) {
        double area = cv::contourArea(contour);
        double perimeter = cv::arcLength(contour, true);
        
        if (perimeter == 0) return 0;
        
        // 圆形度公式: 4π * 面积 / 周长²
        // 完美圆形的值为1，其他形状小于1
        double circularity = (4 * CV_PI * area) / (perimeter * perimeter);
        return circularity;
    }
    
    // 检测绿色圆形
    cv::Mat detectGreenCircles(const cv::Mat& frame) {
        // 保存当前帧
        frame.copyTo(current_frame_);
        
        // 转换为HSV颜色空间
        cv::Mat hsv;
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        
        // 创建绿色掩码
        cv::inRange(hsv, green_lower_, green_upper_, green_mask_);
        
        // 形态学操作（去噪）
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(green_mask_, green_mask_, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(green_mask_, green_mask_, cv::MORPH_CLOSE, kernel);
        
        // 复制原始图像用于显示结果
        cv::Mat result;
        frame.copyTo(result);
        
        // 查找轮廓
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(green_mask_, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        int green_circle_count = 0;
        
        for (size_t i = 0; i < contours.size(); i++) {
            double area = cv::contourArea(contours[i]);
            
            // 1. 面积过滤
            if (area < min_area_ || area > max_area_) {
                continue;
            }
            
            // 2. 圆形度检测
            double circularity = calculateCircularity(contours[i]);
            if (circularity < circularity_threshold_) {
                continue;  // 不是圆形，跳过
            }
            
            // 3. 使用最小外接圆进一步验证
            cv::Point2f center;
            float radius;
            cv::minEnclosingCircle(contours[i], center, radius);
            
            // 半径过滤
            if (radius < min_radius_ || radius > max_radius_) {
                continue;
            }
            
            // 4. 椭圆拟合（进一步验证圆形）
            if (contours[i].size() >= 5) {  // 需要至少5个点来拟合椭圆
                cv::RotatedRect ellipse = cv::fitEllipse(contours[i]);
                
                // 计算椭圆的纵横比
                double aspect_ratio = ellipse.size.width / ellipse.size.height;
                
                // 如果是圆形，纵横比应该接近1（0.8-1.2）
                if (aspect_ratio < 0.8 || aspect_ratio > 1.2) {
                    continue;
                }
            }
            
            // 绘制圆形轮廓
            cv::drawContours(result, contours, static_cast<int>(i), cv::Scalar(0, 255, 0), 2);
            
            // 绘制最小外接圆
            cv::circle(result, center, static_cast<int>(radius), cv::Scalar(0, 0, 255), 2);
            
            // 绘制圆心
            cv::circle(result, center, 3, cv::Scalar(255, 0, 0), -1);
            
            // 计算轮廓的矩以获取质心
            cv::Moments M = cv::moments(contours[i]);
            if (M.m00 != 0) {
                cv::Point centroid(static_cast<int>(M.m10 / M.m00), 
                                   static_cast<int>(M.m01 / M.m00));
                
                // 绘制质心
                cv::circle(result, centroid, 3, cv::Scalar(255, 255, 0), -1);
                
                // 绘制标签
                std::string label = "Circle #" + std::to_string(green_circle_count + 1);
                cv::putText(result, label, 
                           cv::Point(static_cast<int>(center.x - radius), 
                                     static_cast<int>(center.y - radius - 10)),
                           cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);
                
                // 显示详细信息
                std::string info = "R:" + std::to_string(static_cast<int>(radius)) + 
                                   " C:" + std::to_string(static_cast<int>(circularity * 100)) + "%";
                cv::putText(result, info, 
                           cv::Point(static_cast<int>(center.x - radius), 
                                     static_cast<int>(center.y + radius + 20)),
                           cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
                
                // 输出到控制台
                std::cout << "检测到绿色圆形灯 #" << (green_circle_count + 1) 
                          << " - 半径: " << radius 
                          << ", 圆形度: " << circularity 
                          << ", 面积: " << area 
                          << ", 中心: (" << center.x << ", " << center.y << ")" << std::endl;
                
                green_circle_count++;
            }
        }
        
        // 显示掩码
        cv::imshow("Green Mask", green_mask_);
        
        // 在图像顶部显示统计信息
        std::string stats = "绿色圆形灯数量: " + std::to_string(green_circle_count);
        cv::putText(result, stats, 
                   cv::Point(10, result.rows - 10), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
        
        // 显示当前参数
        std::string param_info = "圆形度阈值: " + std::to_string(circularity_threshold_).substr(0, 4);
        cv::putText(result, param_info, 
                   cv::Point(10, result.rows - 40), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 1);
        
        return result;
    }
    
    // 可选的霍夫圆检测方法（备用）
    void detectByHoughCircles(const cv::Mat& frame, cv::Mat& result) {
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        
        // 高斯模糊以减少噪声
        cv::GaussianBlur(gray, gray, cv::Size(9, 9), 2, 2);
        
        std::vector<cv::Vec3f> circles;
        
        // 霍夫圆检测
        cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 
                        1,  // 累加器分辨率
                        gray.rows/8,  // 最小圆心间距
                        100,  // Canny边缘检测阈值
                        30,   // 累加器阈值
                        static_cast<int>(min_radius_),  // 最小半径
                        static_cast<int>(max_radius_)); // 最大半径
        
        // 绘制检测到的圆
        for (size_t i = 0; i < circles.size(); i++) {
            cv::Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
            int radius = cvRound(circles[i][2]);
            
            // 检查圆心区域是否为绿色
            cv::Rect roi(center.x - radius, center.y - radius, 
                        2 * radius, 2 * radius);
            roi &= cv::Rect(0, 0, frame.cols, frame.rows);  // 确保在图像内
            
            if (roi.width > 0 && roi.height > 0) {
                cv::Mat roi_hsv, roi_mask;
                cv::cvtColor(frame(roi), roi_hsv, cv::COLOR_BGR2HSV);
                cv::inRange(roi_hsv, green_lower_, green_upper_, roi_mask);
                
                double green_ratio = static_cast<double>(cv::countNonZero(roi_mask)) / (roi.width * roi.height);
                
                if (green_ratio > 0.3) {  // 如果绿色比例足够高
                    // 绘制圆
                    cv::circle(result, center, radius, cv::Scalar(0, 255, 0), 2);
                    cv::circle(result, center, 3, cv::Scalar(0, 0, 255), -1);
                    
                    std::cout << "霍夫圆检测: 半径=" << radius 
                              << ", 中心=(" << center.x << "," << center.y << ")"
                              << ", 绿色比例=" << green_ratio << std::endl;
                }
            }
        }
    }
    
    void saveCurrentFrame() {
        if (!current_frame_.empty()) {
            std::string filename = "green_circle_" + std::to_string(frame_counter_++) + ".jpg";
            cv::imwrite(filename, current_frame_);
            std::cout << "已保存图像: " << filename << std::endl;
            
            // 同时保存掩码
            if (!green_mask_.empty()) {
                std::string mask_filename = "green_mask_" + std::to_string(frame_counter_) + ".jpg";
                cv::imwrite(mask_filename, green_mask_);
                std::cout << "已保存掩码: " << mask_filename << std::endl;
            }
        }
    }
    
    void resetParameters() {
        init_green_range();
        circularity_threshold_ = 0.7;
        std::cout << "参数已重置" << std::endl;
    }
};

int main() {
    try {
        // 初始化摄像头配置
        sensor::camera::CAM_INFO camInfo;
        camInfo.setCamID(0)           // 设置相机ID
               .setWidth(1440)        // 设置图像宽度
               .setHeight(1080)       // 设置图像高度
               .setOffsetX(0)         // 设置X偏移
               .setOffsetY(0)         // 设置Y偏移
               .setExpTime(5000.0f)   // 设置曝光时间
               .setGain(16.0f)        // 设置增益
               .setTrigger(sensor::camera::SOFTWARE)
               .setGamma(sensor::camera::sRGB);

        // 创建海康摄像头实例
        sensor::camera::HikCam camera(camInfo);
        
        // 创建绿色圆形检测器
        GreenCircleDetector detector;
        
        std::cout << "前哨站绿色圆形灯识别系统启动" << std::endl;
        std::cout << "==========================================" << std::endl;
        
        while (true) {
            // 捕获图像
            cv::Mat frame = camera.Grab();
            
            if (!frame.empty()) {
                // 处理图像
                detector.processFrame(frame);
            } else {
                std::cout << "获取图像失败!" << std::endl;
            }
            
            // 检查按键
            int key = cv::waitKey(30);
            
            // 处理按键输入
            detector.handleKeyPress(key);
            
            // 按ESC键或'q'键退出
            if (key == 27 || key == 'q' || key == 'Q') {
                break;
            }
        }
        
        std::cout << "\n程序结束" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误发生: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}