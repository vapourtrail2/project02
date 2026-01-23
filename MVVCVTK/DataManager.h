#pragma once
#include "AppInterfaces.h"
#include <vector>
#include <array>

class RawVolumeDataManager : public AbstractDataManager {
private:
    vtkSmartPointer<vtkImageData> m_vtkImage;      // VTK 包装对象
    int m_dims[3] = { 0, 0, 0 };
    double m_spacing = 0.02125;

public:
    RawVolumeDataManager();
    bool LoadData(const std::string& filePath) override;
    vtkSmartPointer<vtkImageData> GetVtkImage() const override;
    bool SetFromBuffer(const float* data,
        const std::array<int, 3 >& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin);
};
