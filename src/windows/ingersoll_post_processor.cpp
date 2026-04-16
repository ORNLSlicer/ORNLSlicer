#if 0
    #include "windows/ingersoll_post_processor.h"

    #include "QFile"
    #include "QFileDialog"
    #include "managers/session_manager.h"
    #include "windows/main_window.h"

namespace ORNL
{
  IngersollPostProcessor::IngersollPostProcessor(QWidget* parent)
    : QWidget ()
  {
      parentWindow = parent;

      //resizing makes no point for this tool
      setFixedSize(360,130);
      setWindowTitle("ORNLSlicer-" BOOST_PP_STRINGIZE(ORNLSLICER_VERSION) ": Ingersoll Post Processor");
      QIcon icon;
      icon.addFile(QStringLiteral(":/icons/ornl.png"), QSize(), QIcon::Normal, QIcon::Off);
      setWindowIcon(icon);

      m_layout = new QGridLayout();

      //Description copied from Slicer-1
      QString description = "The Ingersoll Post Processor takes an input GCode file and modifies it ";
      description += "for final use. Each line of the GCode will be numbered and given an ";
      description += "\"N\" prefix. The restart markers will be added so that the start of each ";
      description += "new bead is labeled with a bead number and a layer number.";
      QLabel *lblDescription = new QLabel(description);
      lblDescription->setWordWrap(true);

      m_layout->addWidget(lblDescription, 0,0);

      m_openFile2Process = new QPushButton("Open GCode File to Post Process");
      m_layout->addWidget(m_openFile2Process, 1,0);

      this->setLayout(m_layout);
      this->setupEvents();

      //The default path for the file dialog:
      //   last GCode save directory
      //   last model loaded if a model has been loaded
      //   Qt standard application location
      savedPath=CSM->getSavedGCodePath();
      if(savedPath.isEmpty())
      {
          if(CSM->parts().count()>0)
              savedPath=QFileInfo(CSM->parts().last()->file()).absolutePath();
          else
              savedPath=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      }

      // Status bar.
      m_statusbar = new QStatusBar(this);
  }

  IngersollPostProcessor::~IngersollPostProcessor()
  {

  }

  void IngersollPostProcessor::setupEvents()
  {
        connect(m_openFile2Process, SIGNAL(clicked()), this, SLOT(openFile2Process()));
  }

  void IngersollPostProcessor::openFile2Process()
  {
      QString filter("GCode File (*.mpf)");
      QString selFilter = "GCode File (*.mpf)";

      QString openfilepath = QFileDialog::getOpenFileName(this, tr("Open GCode file"), savedPath, filter, &selFilter);
      if(openfilepath.isEmpty())
      {
          m_statusbar->showMessage("Request cancelled by user...", 5000);
          m_layout->addWidget(m_statusbar, 2, 0);
      }
      else
      {
          //check the file extension
          QString extension = QFileInfo(openfilepath).completeSuffix();
          if(extension.toLower() != "mpf")
          {
              m_statusbar->showMessage("Input file must be of .mpf type ...", 5000);
              m_layout->addWidget(m_statusbar, 2, 0);
              return;
          }
          savedPath=QFileInfo(openfilepath).absolutePath();
          m_statusbar->showMessage("GCode file chosen, now pick a file name to save to ...", 5000);
          m_layout->addWidget(m_statusbar, 2, 0);

          QString savefilepath = QFileDialog::getSaveFileName(this, tr("Save GCode file"), savedPath, filter, &selFilter);
          if(savefilepath.isEmpty())
          {
              m_statusbar->showMessage("Request cancelled by user...", 5000);
              m_layout->addWidget(m_statusbar, 2, 0);
              return;
          }
          else
          {
              QString fileName=QFileInfo(savefilepath).fileName();
              bool status = processFile(openfilepath, savefilepath);
              if(status)
              {
                  m_statusbar->showMessage(fileName + " successfully generated", 5000);
                  m_layout->addWidget(m_statusbar, 2, 0);
              }
              else
              {
                  m_statusbar->showMessage("Error in writing to " + fileName, 5000);
                  m_layout->addWidget(m_statusbar, 2, 0);
              }
          }
      }
  }

  bool IngersollPostProcessor::processFile(QString inputFileName, QString outputFileName)
  {
      //post processing goes here
      QFile inputFile(inputFileName);
      QStringList inputFileContents;
      if (inputFile.open(QIODevice::ReadOnly))
      {
          QTextStream in(&inputFile);
          QString text = in.readAll();
          inputFileContents=text.split('\n');
          inputFile.close();
      }

      QFile outputFile(outputFileName);
      if(!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
          m_statusbar->showMessage("Cannot open " + outputFileName +" to write ...", 5000);
          m_layout->addWidget(m_statusbar, 2, 0);
          return false;
      }
      QTextStream out(&outputFile);

      //start to populate the destination file, code ported from Slicer-1
      int layNum = 0, beadNum = 0, lineNum = 1, loc;
      QString numLayers="";
      out << "N" + QString::number(lineNum) + " def real Cycle_Time = 0" << "\n";
      for(QString line : inputFileContents)
      {
          lineNum++;
          if(line.contains("; Layer count"))
          {
              loc=line.indexOf("nt:")+4;
              numLayers = line.mid(loc, line.length() - loc - 1); //line contains '\n' at the end
              out << "N" + QString::number(lineNum) + " " + line;
          }
          else if (line.contains("; BEGINNING"))
          {
              layNum++;
              beadNum = 1;
              out << "N" + QString::number(lineNum) + " " + line;
              lineNum++;
              out << "N" + QString::number(lineNum) + " LAY" + QString::number(layNum) + "_BEAD1: WHAM_LAYER=" + QString::number(layNum) + " WHAM_BEAD=1" + "\n";
              lineNum++;
              out << "N" + QString::number(lineNum) + " MSG(\"LAYER " + QString::number(layNum) + " of " + numLayers + " - Previous Layer Time = \"<<($AC_CYCLE_TIME - Cycle_Time)<<\" Seconds\")\n";
              lineNum++;
              out << "N" + QString::number(lineNum) + " Cycle_Time = $AC_CYCLE_TIME\n";
          }
          else if (line.contains("Lift Tip") && beadNum==1)
          {
              out << "N" + QString::number(lineNum) + " " + line;
              beadNum++;
          }
          else if (line.contains("Lift Tip") && beadNum > 1)
          {
              out << "N" + QString::number(lineNum) + " LAY" + QString::number(layNum) + "_BEAD" + QString::number(beadNum) + ": WHAM_LAYER=" + QString::number(layNum) + " WHAM_BEAD=" + QString::number(beadNum) + "\n";
              out << "N" + QString::number(lineNum) + " " + line;
              beadNum++;
          }
          else if(lineNum < inputFileContents.count()-1 || line.size()>0)
          {
              //the conditional clauses are here to include all empty lines except the last one
              out << "N" + QString::number(lineNum) + " " + line;
          }
      }

      return true;
  }

  void IngersollPostProcessor::closeEvent(QCloseEvent *event)
  {
      parentWindow->setFocus();
      if(parentWindow->isMinimized()) parentWindow->showNormal();
      parentWindow->activateWindow();
  }

}
#endif
