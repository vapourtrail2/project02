#pragma once

#include "AppInterfaces.h"
#include "DataConverters.h"
#include <memory>
#include <vtkTable.h>

class VolumeAnalysisService {
private:
    std::shared_ptr<AbstractDataManager> m_dataManager;

public:
    explicit VolumeAnalysisService(std::shared_ptr<AbstractDataManager> dataMgr)
        : m_dataManager(std::move(dataMgr))
    {
    }

    vtkSmartPointer<vtkTable> GetHistogramData(int binCount = 2048) const
    {
        if (!m_dataManager || !m_dataManager->GetVtkImage()) {
            return nullptr;
        }

        auto converter = std::make_shared<HistogramConverter>();
        converter->SetParameter("BinCount", static_cast<double>(binCount));
        return converter->Process(m_dataManager->GetVtkImage());
    }
};
