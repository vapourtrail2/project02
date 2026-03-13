#include "DataManager.h"

#include "MemMappedFile.h"
#include <vtkStringArray.h>
#include <vtkTIFFReader.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

namespace {
bool ParseVolumeDims(const std::string& filePath, int dims[3])
{
    std::filesystem::path pathObj(filePath);
    const std::string name = pathObj.filename().string();

    std::regex pattern(R"((\d+)[xX](\d+)[xX](\d+))");
    std::smatch matches;
    if (!std::regex_search(name, matches, pattern) || matches.size() <= 3) {
        return false;
    }

    dims[0] = std::stoi(matches[1].str());
    dims[1] = std::stoi(matches[2].str());
    dims[2] = std::stoi(matches[3].str());
    return dims[0] > 0 && dims[1] > 0 && dims[2] > 0;
}
}

RawVolumeDataManager::RawVolumeDataManager()
{
    m_vtkImage = vtkSmartPointer<vtkImageData>::New();
}

bool RawVolumeDataManager::LoadData(const std::string& filePath)
{
    auto setState = [this](LoadState state, bool isLoading) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_loadState = state;
        }
        m_isLoading.store(isLoading);
    };

    namespace fs = std::filesystem;
    setState(LoadState::Loading, true);

    std::error_code ec;
    if (filePath.empty() || !fs::exists(filePath, ec) || ec || !fs::is_regular_file(filePath, ec) || ec) {
        setState(LoadState::Failed, false);
        return false;
    }

    int newDims[3] = { 0, 0, 0 };
    if (!ParseVolumeDims(filePath, newDims)) {
        setState(LoadState::Failed, false);
        return false;
    }

    const size_t voxels =
        static_cast<size_t>(newDims[0]) *
        static_cast<size_t>(newDims[1]) *
        static_cast<size_t>(newDims[2]);
    if (voxels == 0) {
        setState(LoadState::Failed, false);
        return false;
    }

    const uintmax_t fileBytes = fs::file_size(filePath, ec);
    if (ec) {
        setState(LoadState::Failed, false);
        return false;
    }

    const size_t floatBytes = voxels * sizeof(float);
    const size_t u16Bytes = voxels * sizeof(uint16_t);
    const bool isFloat = fileBytes == floatBytes;
    const bool isU16 = fileBytes == u16Bytes;
    if (!isFloat && !isU16) {
        setState(LoadState::Failed, false);
        return false;
    }

    auto newImage = vtkSmartPointer<vtkImageData>::New();
    newImage->SetDimensions(newDims[0], newDims[1], newDims[2]);
    newImage->SetSpacing(m_spacing, m_spacing, m_spacing);
    newImage->SetOrigin(0.0, 0.0, 0.0);
    newImage->AllocateScalars(VTK_FLOAT, 1);

    float* dst = static_cast<float*>(newImage->GetScalarPointer());
    std::fill_n(dst, voxels, 0.0f);

    MemMappedFile mmf;
    if (mmf.open(filePath)) {
        if (isFloat) {
            std::memcpy(dst, mmf.data(), floatBytes);
        } else {
            const auto* src = static_cast<const uint16_t*>(mmf.data());
            for (size_t i = 0; i < voxels; ++i) {
                dst[i] = static_cast<float>(src[i]);
            }
        }
    } else {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            setState(LoadState::Failed, false);
            return false;
        }

        if (isFloat) {
            file.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(floatBytes));
            if (file.gcount() != static_cast<std::streamsize>(floatBytes)) {
                setState(LoadState::Failed, false);
                return false;
            }
        } else {
            std::vector<uint16_t> tmp(voxels);
            file.read(reinterpret_cast<char*>(tmp.data()), static_cast<std::streamsize>(u16Bytes));
            if (file.gcount() != static_cast<std::streamsize>(u16Bytes)) {
                setState(LoadState::Failed, false);
                return false;
            }
            for (size_t i = 0; i < voxels; ++i) {
                dst[i] = static_cast<float>(tmp[i]);
            }
        }
    }

    newImage->Modified();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_vtkImage = newImage;
        m_dims[0] = newDims[0];
        m_dims[1] = newDims[1];
        m_dims[2] = newDims[2];
    }

    setState(LoadState::Succeeded, false);
    return true;
}

vtkSmartPointer<vtkImageData> RawVolumeDataManager::GetVtkImage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vtkImage;
}

bool RawVolumeDataManager::SetFromBuffer(
    const float* data,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin)
{
    auto setState = [this](LoadState state, bool isLoading) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_loadState = state;
        }
        m_isLoading.store(isLoading);
        };

    if (!data || dims[0] <= 0 || dims[1] <= 0 || dims[2] <= 0) {
        setState(LoadState::Failed, false);
        return false;
    }

    setState(LoadState::Loading, true);

    // ── 在调用方线程完成唯一一次分配 + 拷贝（只此一次，不再重复）────
    auto newImage = vtkSmartPointer<vtkImageData>::New();
    newImage->SetDimensions(dims[0], dims[1], dims[2]);
    newImage->SetSpacing(spacing[0], spacing[1], spacing[2]);
    newImage->SetOrigin(origin[0], origin[1], origin[2]);
    newImage->AllocateScalars(VTK_FLOAT, 1);   // 分配（page-fault 在此触发）

    const size_t total =
        static_cast<size_t>(dims[0]) *
        static_cast<size_t>(dims[1]) *
        static_cast<size_t>(dims[2]);
    float* dst = static_cast<float*>(newImage->GetScalarPointer());
    std::memcpy(dst, data, total * sizeof(float));  // 唯一一次拷贝

    // Modified() 暂不调用——留到主线程 ConsumeReconImage() 中调用，
    // 确保 VTK pipeline 脏标记传播在正确线程触发。

    {
        std::lock_guard<std::mutex> lock(m_reconMutex);
        m_pendingImage = std::move(newImage);   // 指针赋值，无拷贝
    }
    m_hasPendingImage.store(true);

    // LoadState::Succeeded 延迟到主线程 ConsumeReconImage() 设置，
    // 防止调用方在 vtkImage 还未提交时就查询到 Succeeded 状态。
    return true;
}

bool RawVolumeDataManager::ConsumeReconImage()
{
    if (!m_hasPendingImage.load()) return false;

    vtkSmartPointer<vtkImageData> incoming;
    {
        std::lock_guard<std::mutex> lock(m_reconMutex);
        if (!m_pendingImage) return false;
        incoming = std::move(m_pendingImage);
        m_hasPendingImage.store(false);
    }

    // Modified() 在主线程调用（VTK pipeline 脏传播安全）
    incoming->Modified();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_vtkImage = incoming;
        m_dims[0] = incoming->GetDimensions()[0];
        m_dims[1] = incoming->GetDimensions()[1];
        m_dims[2] = incoming->GetDimensions()[2];
        m_spacing = incoming->GetSpacing()[0];
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_loadState = LoadState::Succeeded;
    }
    m_isLoading.store(false);

    return true;
}

//bool RawVolumeDataManager::SetFromBuffer(//recon links
//    const float* data,
//    const std::array<int, 3>& dims,
//    const std::array<float, 3>& spacing,
//    const std::array<float, 3>& origin)
//{
//    auto setState = [this](LoadState state, bool isLoading) {
//        {
//            std::lock_guard<std::mutex> lock(m_stateMutex);
//            m_loadState = state;
//        }
//        m_isLoading.store(isLoading);
//    };
//
//    if (!data || dims[0] <= 0 || dims[1] <= 0 || dims[2] <= 0) {
//        setState(LoadState::Failed, false);
//        return false;
//    }
//
//    auto newImage = vtkSmartPointer<vtkImageData>::New();
//    newImage->SetDimensions(dims[0], dims[1], dims[2]);
//    newImage->SetSpacing(spacing[0], spacing[1], spacing[2]);
//    newImage->SetOrigin(origin[0], origin[1], origin[2]);
//    newImage->AllocateScalars(VTK_FLOAT, 1);
//
//    const size_t total =
//        static_cast<size_t>(dims[0]) *
//        static_cast<size_t>(dims[1]) *
//        static_cast<size_t>(dims[2]);
//    float* dst = static_cast<float*>(newImage->GetScalarPointer());
//    std::memcpy(dst, data, total * sizeof(float));
//    newImage->Modified();
//
//    {
//        std::lock_guard<std::mutex> lock(m_mutex);
//        m_vtkImage = newImage;
//        m_dims[0] = dims[0];
//        m_dims[1] = dims[1];
//        m_dims[2] = dims[2];
//        m_spacing = spacing[0];
//    }
//
//    setState(LoadState::Succeeded, false);
//    return true;
//}

TiffVolumeDataManager::TiffVolumeDataManager()
{
    m_vtkImage = vtkSmartPointer<vtkImageData>::New();
}

bool TiffVolumeDataManager::LoadData(const std::string& inputPath)
{
    auto setState = [this](LoadState state, bool isLoading) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_loadState = state;
        }
        m_isLoading.store(isLoading);
    };

    namespace fs = std::filesystem;
    setState(LoadState::Loading, true);

    fs::path pathObj(inputPath);
    if (!fs::exists(pathObj)) {
        setState(LoadState::Failed, false);
        return false;
    }

    auto reader = vtkSmartPointer<vtkTIFFReader>::New();
    if (fs::is_directory(pathObj)) {
        std::vector<std::string> fileList;
        for (const auto& entry : fs::directory_iterator(pathObj)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            if (ext == ".tif" || ext == ".tiff") {
                fileList.push_back(entry.path().string());
            }
        }

        if (fileList.empty()) {
            setState(LoadState::Failed, false);
            return false;
        }

        auto naturalSort = [](const std::string& lhs, const std::string& rhs) {
            size_t i = 0;
            size_t j = 0;
            while (i < lhs.size() && j < rhs.size()) {
                if (std::isdigit(static_cast<unsigned char>(lhs[i])) && std::isdigit(static_cast<unsigned char>(rhs[j]))) {
                    unsigned long long n1 = 0;
                    unsigned long long n2 = 0;
                    while (i < lhs.size() && std::isdigit(static_cast<unsigned char>(lhs[i]))) {
                        n1 = n1 * 10 + static_cast<unsigned long long>(lhs[i] - '0');
                        ++i;
                    }
                    while (j < rhs.size() && std::isdigit(static_cast<unsigned char>(rhs[j]))) {
                        n2 = n2 * 10 + static_cast<unsigned long long>(rhs[j] - '0');
                        ++j;
                    }
                    if (n1 != n2) {
                        return n1 < n2;
                    }
                } else {
                    if (lhs[i] != rhs[j]) {
                        return lhs[i] < rhs[j];
                    }
                    ++i;
                    ++j;
                }
            }
            return lhs.size() < rhs.size();
        };

        std::sort(fileList.begin(), fileList.end(), naturalSort);
        auto vtkFiles = vtkSmartPointer<vtkStringArray>::New();
        for (const auto& file : fileList) {
            vtkFiles->InsertNextValue(file);
        }
        reader->SetFileNames(vtkFiles);
    } else {
        reader->SetFileName(inputPath.c_str());
        if (!reader->CanReadFile(inputPath.c_str())) {
            setState(LoadState::Failed, false);
            return false;
        }
    }

    try {
        reader->Update();
    } catch (...) {
        setState(LoadState::Failed, false);
        return false;
    }

    auto output = reader->GetOutput();
    if (!output || output->GetDimensions()[0] == 0) {
        setState(LoadState::Failed, false);
        return false;
    }

    auto newImage = vtkSmartPointer<vtkImageData>::New();
    newImage->ShallowCopy(output);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_vtkImage = newImage;
    }

    setState(LoadState::Succeeded, false);
    return true;
}

vtkSmartPointer<vtkImageData> TiffVolumeDataManager::GetVtkImage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vtkImage;
}
