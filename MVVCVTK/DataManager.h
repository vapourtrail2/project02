#pragma once

#include "AppInterfaces.h"
#include <array>
#include <mutex>
#include <vector>

class RawVolumeDataManager : public AbstractDataManager {
private:
    mutable std::mutex m_mutex;
    vtkSmartPointer<vtkImageData> m_vtkImage;
    int m_dims[3] = { 0, 0, 0 };
    double m_spacing = 0.02125;

public:
    RawVolumeDataManager();
    bool LoadData(const std::string& filePath) override;
    vtkSmartPointer<vtkImageData> GetVtkImage() const override;
    bool SetFromBuffer(
        const float* data,
        const std::array<int, 3>& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin);
};

class TiffVolumeDataManager : public AbstractDataManager {
private:
    mutable std::mutex m_mutex;
    vtkSmartPointer<vtkImageData> m_vtkImage;

public:
    TiffVolumeDataManager();
    bool LoadData(const std::string& filePath) override;
    vtkSmartPointer<vtkImageData> GetVtkImage() const override;
};
