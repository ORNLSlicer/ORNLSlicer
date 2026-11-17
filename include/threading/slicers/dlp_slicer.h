#pragma once

#include <QStringList>

#include "dlp/dlp_build.h"
#include "threading/traditional_ast.h"

namespace ORNL {
struct DlpPreflight {
    DlpBuild layout;
    int layer_count      = 0;
    qint64 file_count    = 0;
    double working_bytes = 0;
    double output_bytes  = 0;
    QStringList warnings;
};

class DlpSlicer : public TraditionalAST {
   public:
    explicit DlpSlicer(QString outputLocation);

    //! Checks layout and temporary disk capacity without allocating images or temporary storage.
    static bool preflight(DlpPreflight& result, QString* error = nullptr);

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
