#include "DataManager.h"
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <fstream>
#include <filesystem>
#include <regex>
#include <cstring>
#include <vector>   
using namespace std;


RawVolumeDataManager::RawVolumeDataManager() {
    m_vtkImage = vtkSmartPointer<vtkImageData>::New();
}

bool RawVolumeDataManager::LoadData(const std::string& filePath)
{
    namespace fs = std::filesystem;

    if (filePath.empty()) {
        return false;
    }

    std::error_code ec;
    if (!fs::exists(filePath, ec) || ec) {
        return false;
    }

    if (!fs::is_regular_file(filePath, ec) || ec) {
        return false;
    }

    //从文件名解析尺寸 (例如 data_512x512x200.raw) 
    fs::path pathObj(filePath);
    const std::string name = pathObj.filename().string();

    std::regex pattern(R"((\d+)[xX](\d+)[xX](\d+))");
    std::smatch matches;

    if (std::regex_search(name, matches, pattern) && matches.size() > 3) {
        m_dims[0] = std::stoi(matches[1].str());
        m_dims[1] = std::stoi(matches[2].str());
        m_dims[2] = std::stoi(matches[3].str());
    }
    else {
        std::cout << "文件名格式不对" << std::endl;
        return false;
    }

    if (m_dims[0] <= 0 || m_dims[1] <= 0 || m_dims[2] <= 0) {
        return false;
    }

    const size_t voxels =
        static_cast<size_t>(m_dims[0]) *
        static_cast<size_t>(m_dims[1]) *
        static_cast<size_t>(m_dims[2]);
        
    if (voxels == 0) {
        return false;
    }

    const uintmax_t fileBytes = fs::file_size(filePath, ec);//获取filePath文件的大小，以字节为单位
    
    if (ec) {
        return false;
    }

    const uintmax_t finalFloatBytes = static_cast<uintmax_t>(voxels) * sizeof(float);
    const uintmax_t finalU16Bytes = static_cast<uintmax_t>(voxels) * sizeof(uint16_t);

    const bool isFloat = (fileBytes == finalFloatBytes);
    const bool isU16 = (fileBytes == finalU16Bytes);

    if (!isFloat && !isU16) {
        return false; // 体素数和文件大小不匹配
    }

    //构建 vtkImageData
    m_vtkImage->SetDimensions(m_dims[0], m_dims[1], m_dims[2]);
    m_vtkImage->SetSpacing(m_spacing, m_spacing, m_spacing); 
    m_vtkImage->SetOrigin(0, 0, 0);
    m_vtkImage->AllocateScalars(VTK_FLOAT, 1);//每个体素存一个值

    float* out = static_cast<float*>(m_vtkImage->GetScalarPointer());

    //读文件 
	std::ifstream file(filePath, std::ios::binary);//文件以二进制方式打开 不会进行字符转换
    if (!file.is_open()) {
        std::cout << "打开失败" << std::endl;
        return false;
    }

    if (isFloat) {
        file.read(reinterpret_cast<char*>(out),
	    static_cast<std::streamsize>(finalFloatBytes));//read的作用是 从硬盘里文件种抓取指定数量的字节数 原封不动地存储到内存中

        if (file.gcount() != static_cast<std::streamsize>(finalFloatBytes)) {
            return false;
        }
    }
    else {
        std::vector<uint16_t> tmp(voxels);
        file.read(reinterpret_cast<char*>(tmp.data()),
            static_cast<std::streamsize>(finalU16Bytes));

        if (file.gcount() != static_cast<std::streamsize>(finalU16Bytes)) {
            return false;
        }

        for (size_t i = 0; i < voxels; ++i) {
            out[i] = static_cast<float>(tmp[i]);
        }
    }

    m_vtkImage->Modified();
    return true;
}

vtkSmartPointer<vtkImageData> RawVolumeDataManager::GetVtkImage() const {
    return m_vtkImage;
}

bool RawVolumeDataManager::SetFromBuffer(const float* data,
    const std::array<int, 3 >& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin) 
{
    if (!data) {
        return false;
    }

    m_dims[0] = dims[0];
    m_dims[1] = dims[1];
    m_dims[2] = dims[2];

	m_vtkImage->SetDimensions(m_dims[0], m_dims[1], m_dims[2]);
	m_vtkImage->SetSpacing(spacing[0], spacing[1], spacing[2]);
	m_vtkImage->SetOrigin(origin[0], origin[1], origin[2]);
	m_vtkImage->AllocateScalars(VTK_FLOAT, 1); // 申请内存，类型为float，连续一维

    size_t total = size_t(m_dims[0] * m_dims[1] * m_dims[2]);
    float* res = static_cast<float*>(m_vtkImage->GetScalarPointer());
    std::memcpy(res, data, total * sizeof(float));//目标地址 源地址 拷贝字节数(复制的数据总量)

	m_vtkImage->Modified();

	return true;
}


