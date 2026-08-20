#pragma once

#include <string>

#include <QThread>
#include <qobject.h>
#include <qtmetamacros.h>
#include <zip/zip.h>

#include "utilities/qt_json_conversion.h"

namespace ORNL {
/*!
 * \class SessionLoader
 * \brief Saves or loads a session file in a separate thread.
 * \todo The session saving and loading needs some cleanup.
 */
class SessionLoader : public QThread {
    Q_OBJECT
  public:
    //! \brief Constructor.
    //! \param filename: Filename for either loading or saving.
    //! \param save:     If true, the current session will be saved. If false, the current session will be loaded.
    SessionLoader(QString filename, bool save);

    //! \brief Get global settings from zip
    //! \return json from global settings for version check
    fifojson getSettingsFromZip();

    //! \brief Set global settings
    //! \param j: json to override when opening zip
    //! \param writeToProject: if true, persist the override back into the project file
    void updateSettingsJson(fifojson j, bool writeToProject = true);

    //! \brief Start the thread.
    void run() override;

  signals:
    //! \brief Signal that an error has occured.
    void error(QString error);

    //! \brief Signal that a session file has been saved successfully.
    void saveSucceeded();

    //! \brief Signal that a session file has been loaded successfully.
    void loadSucceeded();

  private:
    //! \brief Saves a session.
    void saveSession();

    //! \brief Load session.
    void loadSession();

    //! \brief loads string from zip file
    //! \param zip: the zip file to open from
    //! \param key: the name of the file
    std::string loadStringFromZip(struct zip_t*, const std::string& key);

    //! \brief Filename this loader thread will work on.
    QString m_filename;

    //! \brief If enabled, this save a session rather than load it.
    bool m_save;

    //! \brief json to override global when opening zip
    fifojson m_new_json;

    //! \brief If enabled, m_new_json replaces the archived global settings during load.
    bool m_has_new_json;

    //! \brief If enabled, m_new_json is also written back into the project file.
    bool m_should_write_new_json;

}; // class SessionLoader
} // namespace ORNL
