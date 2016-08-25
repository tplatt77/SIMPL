/* ============================================================================
* Copyright (c) 2009-2016 BlueQuartz Software, LLC
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright notice, this
* list of conditions and the following disclaimer in the documentation and/or
* other materials provided with the distribution.
*
* Neither the name of BlueQuartz Software, the US Air Force, nor the names of its
* contributors may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* The code contained herein was partially funded by the followig contracts:
*    United States Air Force Prime Contract FA8650-07-D-5800
*    United States Air Force Prime Contract FA8650-10-D-5210
*    United States Prime Contract Navy N00173-07-C-2068
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include "DataArrayCreationWidget.h"

#include <QtCore/QMetaProperty>
#include <QtCore/QList>
#include <QtCore/QSignalMapper>

#include <QtGui/QStandardItemModel>

#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QMenu>

#include "SIMPLib/DataContainers/DataArrayPath.h"
#include "SVWidgetsLib/Core/SVWidgetsLibConstants.h"
#include "SVWidgetsLib/QtSupport/QtSStyles.h"

#include "FilterParameterWidgetsDialogs.h"
#include "FilterParameterWidgetUtils.hpp"


// Include the MOC generated file for this class
#include "moc_DataArrayCreationWidget.cpp"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataArrayCreationWidget::DataArrayCreationWidget(FilterParameter* parameter, AbstractFilter* filter, QWidget* parent) :
  FilterParameterWidget(parameter, filter, parent),
  m_DidCausePreflight(false)
{
  m_FilterParameter = dynamic_cast<DataArrayCreationFilterParameter*>(parameter);
  Q_ASSERT_X(m_FilterParameter != NULL, "NULL Pointer", "DataArrayCreationWidget can ONLY be used with a DataArrayCreationFilterParameter object");

  setupUi(this);
  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataArrayCreationWidget::DataArrayCreationWidget(QWidget* parent) :
  FilterParameterWidget(NULL, NULL, parent),
  m_DidCausePreflight(false)
{
  setupUi(this);
  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataArrayCreationWidget::~DataArrayCreationWidget()
{}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::initializeWidget(FilterParameter* parameter, AbstractFilter* filter)
{
  setFilter(filter);
  setFilterParameter(parameter);
  setupGui();
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::setupGui()
{
  blockSignals(true);
  if (getFilterParameter() != NULL)
  {
    QString str = getFilterParameter()->getHumanLabel();
    label->setText(str);
  }
  blockSignals(false);

  applyChangesBtn->setVisible(false);

  // Do not allow the user to put a forward slash into the attributeMatrixName line edit
  dataArrayName->setValidator(new QRegularExpressionValidator(QRegularExpression("[^/]*"), this));

  m_SelectedAttributeMatrixPath->setStyleSheet(QtSStyles::DAPSelectionButtonStyle(true));

  m_MenuMapper = new QSignalMapper(this);
  connect(m_MenuMapper, SIGNAL(mapped(QString)),
            this, SLOT(attributeMatrixSelected(QString)));

  // Catch when the filter is about to execute the preflight
  connect(getFilter(), SIGNAL(preflightAboutToExecute()),
          this, SLOT(beforePreflight()));

  // Catch when the filter is finished running the preflight
  connect(getFilter(), SIGNAL(preflightExecuted()),
          this, SLOT(afterPreflight()));

  // Catch when the filter wants its values updated
  connect(getFilter(), SIGNAL(updateFilterParameters(AbstractFilter*)),
          this, SLOT(filterNeedsInputParameters(AbstractFilter*)));

  connect(dataArrayName, SIGNAL(textEdited(const QString&)),
          this, SLOT(widgetChanged(const QString&)));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString DataArrayCreationWidget::checkStringValues(QString curDcName, QString filtDcName)
{
  if(curDcName.isEmpty() == true && filtDcName.isEmpty() == false)
  {return filtDcName;}
  else if(curDcName.isEmpty() == false && filtDcName.isEmpty() == true)
  {return curDcName;}
  else if(curDcName.isEmpty() == false && filtDcName.isEmpty() == false && m_DidCausePreflight == true)
  { return curDcName;}

  return filtDcName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::createSelectionMenu()
{
  // Now get the DataContainerArray from the Filter instance
  // We are going to use this to get all the current DataContainers
  DataContainerArray::Pointer dca = getFilter()->getDataContainerArray();
  if(NULL == dca.get()) { return; }

  // Get the menu and clear it out
  QMenu* menu = m_SelectedAttributeMatrixPath->menu();
  if(!menu)
  {
    menu = new QMenu();
    m_SelectedAttributeMatrixPath->setMenu(menu);
    menu->installEventFilter(this);
  }
  if(menu) {
    menu->clear();
  }

  // Get the DataContainerArray object
  // Loop over the data containers until we find the proper data container
  QList<DataContainer::Pointer> containers = dca->getDataContainers();
  QVector<unsigned int> amTypes = m_FilterParameter->getDefaultAttributeMatrixTypes();
  QVector<unsigned int> geomTypes = m_FilterParameter->getDefaultGeometryTypes();



  QListIterator<DataContainer::Pointer> containerIter(containers);
  while(containerIter.hasNext())
  {
    DataContainer::Pointer dc = containerIter.next();

    IGeometry::Pointer geom = IGeometry::NullPointer();
    uint32_t geomType = 999;
    if (NULL != dc.get()) { geom = dc->getGeometry(); }
    if (NULL != geom.get()) { geomType = geom->getGeometryType(); }


    QMenu* dcMenu = new QMenu(dc->getName());
    dcMenu->setDisabled(false);
    menu->addMenu(dcMenu);
    if(geomTypes.isEmpty() == false && geomTypes.contains(geomType) == false )
    {
      dcMenu->setDisabled(true);
    }


    // We found the proper Data Container, now populate the AttributeMatrix List
    DataContainer::AttributeMatrixMap_t attrMats = dc->getAttributeMatrices();
    QMapIterator<QString, AttributeMatrix::Pointer> attrMatsIter(attrMats);
    while(attrMatsIter.hasNext() )
    {
      attrMatsIter.next();
      QString amName = attrMatsIter.key();
      AttributeMatrix::Pointer am = attrMatsIter.value();

      QAction* action = new QAction(amName, dcMenu);
      DataArrayPath daPath(dc->getName(), amName, "");
      QString path = daPath.serialize(Detail::Delimiter);
      action->setData(path);

      connect(action, SIGNAL(triggered(bool)), m_MenuMapper, SLOT(map()));
      m_MenuMapper->setMapping(action, path);
      dcMenu->addAction(action);

      bool amIsNotNull = (nullptr != am.get()) ? true : false;
      bool amValidType = (amTypes.isEmpty() == false && amTypes.contains(am->getType()) == false) ? true : false;

      if (amIsNotNull && amValidType)
      {
        action->setDisabled(true);
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DataArrayCreationWidget::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::Show && obj == m_SelectedAttributeMatrixPath->menu())
  {
    QPoint pos = adjustedMenuPosition(m_SelectedAttributeMatrixPath);
    m_SelectedAttributeMatrixPath->menu()->move(pos);
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::attributeMatrixSelected(QString path)
{
  m_SelectedAttributeMatrixPath->setText(path);

  DataContainerArray::Pointer dca = getFilter()->getDataContainerArray();
  if(NULL == dca.get()) { return; }


  IDataArray::Pointer attrArray = dca->getPrereqIDataArrayFromPath<IDataArray, AbstractFilter>(getFilter(), DataArrayPath::Deserialize(path, Detail::Delimiter));
  if(nullptr != attrArray.get()) {
    QString html = attrArray->getInfoString(SIMPL::HtmlFormat);
    m_SelectedAttributeMatrixPath->setToolTip(html);
  }

  m_DidCausePreflight = true;
  emit parametersChanged();
  m_DidCausePreflight = false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::on_dataArrayName_returnPressed()
{
  on_applyChangesBtn_clicked();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::hideButton()
{
  dataArrayName->setToolTip("");
  applyChangesBtn->setVisible(false);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::widgetChanged(const QString& text)
{
  dataArrayName->setStyleSheet(QString::fromLatin1("color: rgb(255, 0, 0);"));
  dataArrayName->setToolTip("Press the 'Return' key to apply your changes");
  if(applyChangesBtn->isVisible() == false)
  {
    applyChangesBtn->setVisible(true);
    fadeInWidget(applyChangesBtn);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::setSelectedPath(QString path)
{
  DataArrayPath amPath = DataArrayPath::Deserialize(path, Detail::Delimiter);
  if (amPath.isEmpty()) { return; }

  m_SelectedAttributeMatrixPath->setText("");
  m_SelectedAttributeMatrixPath->setToolTip("");

  DataContainerArray::Pointer dca = getFilter()->getDataContainerArray();
  if(NULL == dca.get()) { return; }

  int err = 0;
  AttributeMatrix::Pointer attrMat = dca->getPrereqAttributeMatrixFromPath(getFilter(), amPath, err);
  if(nullptr != attrMat.get()) {
    QString html = attrMat->getInfoString(SIMPL::HtmlFormat);
    m_SelectedAttributeMatrixPath->setToolTip(html);
    m_SelectedAttributeMatrixPath->setText(path);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::beforePreflight()
{
  if (NULL == getFilter()) { return; }
  if(m_DidCausePreflight == true)
  {
   // std::cout << "***  DataArrayCreationWidget already caused a preflight, just returning" << std::endl;
    return;
  }

  DataArrayPath path = getFilter()->property(PROPERTY_NAME_AS_CHAR).value<DataArrayPath>();

  dataArrayName->setText(path.getDataArrayName());
  path.setDataArrayName("");
  setSelectedPath(path.serialize(Detail::Delimiter));
  createSelectionMenu();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::afterPreflight()
{
  // std::cout << "After Preflight" << std::endl;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::filterNeedsInputParameters(AbstractFilter* filter)
{
  // Generate the path to the AttributeArray
  DataArrayPath selectedPath = DataArrayPath::Deserialize(m_SelectedAttributeMatrixPath->text(), Detail::Delimiter);
  selectedPath.setDataArrayName(dataArrayName->text());
  QVariant var;
  var.setValue(selectedPath);
  bool ok = false;
  // Set the value into the Filter
  ok = filter->setProperty(PROPERTY_NAME_AS_CHAR, var);
  if(false == ok)
  {
    FilterParameterWidgetsDialogs::ShowCouldNotSetFilterParameter(getFilter(), getFilterParameter());
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataArrayCreationWidget::on_applyChangesBtn_clicked()
{
  m_DidCausePreflight = true;
  dataArrayName->setStyleSheet(QString(""));
  emit parametersChanged();

  QPointer<QtSFaderWidget> faderWidget = new QtSFaderWidget(applyChangesBtn);
  setFaderWidget(faderWidget);

  if (getFaderWidget())
  {
    faderWidget->close();
  }
  faderWidget = new QtSFaderWidget(applyChangesBtn);
  faderWidget->setFadeOut();
  connect(faderWidget, SIGNAL(animationComplete() ),
          this, SLOT(hideButton()));
  faderWidget->start();
  m_DidCausePreflight = false;
}
