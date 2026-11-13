#pragma once

#include "dlp/dlp_build.h"
#include "threading/traditional_ast.h"

namespace ORNL {
class DlpSlicer : public TraditionalAST {
   public:
    explicit DlpSlicer(QString outputLocation);

    QSharedPointer<const DlpBuild> build() const {
        return m_build;
    }

   protected:
    void preProcess(nlohmann::json opt_data = nlohmann::json()) override;
    void postProcess(nlohmann::json opt_data = nlohmann::json()) override;
    void writeGCode() override;

   private:
    QSharedPointer<DlpBuild> m_build;
};
}  // namespace ORNL
