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

#include "AttributeMatrixCreationWidget.h"

#include <QtCore/QSignalMapper>
#include <QtCore/QMetaProperty>
#include <QtCore/QList>

#include <QtGui/QStandardItemModel>

#include <QtWidgets/QMenu>
#include <QtWidgets/QListWidgetItem>

#include "SIMPLib/DataContainers/DataArrayPath.h"
#include "SVWidgetsLib/Core/SVWidgetsLibConstants.h"
#include "SVWidgetsLib/QtSupport/QtSStyles.h"

#include "FilterParameterWidgetsDialogs.h"
#include "FilterParameterWidgetUtils.hpp"

// Include the MOC generated file for this class
#include "moc_AttributeMatrixCreationWidget.cpp"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AttributeMatrixCreationWidget::AttributeMatrixCreationWidget(FilterParameter* parameter, AbstractFilter* filter, QWidget* parent) :
FilterParameterWidget(parameter, filter, parent),
m_DidCausePreflight(false)
{
  m_FilterParameter = dynamic_cast<AttributeMatrixCreationFilterParameter*>(parameter);
  Q_ASSERT_X(m_FilterParameter != NULL, "NULL Pointer", "AttributeMatrixCreationWidget can ONLY be used with a AttributeMatrixCreationFilterParameter object");

  setupUi(this);
  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AttributeMatrixCreationWidget::AttributeMatrixCreationWidget(QWidget* parent) :
FilterParameterWidget(NULL, NULL, parent),
m_DidCausePreflight(false)
{
  setupUi(this);
  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AttributeMatrixCreationWidget::~AttributeMatrixCreationWidget()
{}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::initializeWidget(FilterParameter* parameter, AbstractFilter* filter)
{
  setFilter(filter);
  setFilterParameter(parameter);
  setupGui();
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::setupGui()
{
  blockSignals(true);
  if (getFilterParameter() != NULL)
  {
    label->setText(getFilterParameter()->getHumanLabel());

    QString str = getFilter()->property(PROPERTY_NAME_AS_CHAR).toString();
    attributeMatrixName->setText(str);
  }
  blockSignals(false);

  // Do not allow the user to put a forward slash into the attributeMatrixName line edit
  attributeMatrixName->setValidator(new QRegularExpressionValidator(QRegularExpression("[^/]*"), this));

  m_SelectedDataContainerPath->setStyleSheet(QtSStyles::DAPSelectionButtonStyle(true));

  m_MenuMapper = new QSignalMapper(this);
  connect(m_MenuMapper, SIGNAL(mapped(QString)),
            this, SLOT(dataContainerSelected(QString)));

  // Catch when the filter is about to execute the preflight
  connect(getFilter(), SIGNAL(preflightAboutToExecute()),
    this, SLOT(beforePreflight()));

  // Catch when the filter is finished running the preflight
  connect(getFilter(), SIGNAL(preflightExecuted()),
    this, SLOT(afterPreflight()));

  // Catch when the filter wants its values updated
  connect(getFilter(), SIGNAL(updateFilterParameters(AbstractFilter*)),
    this, SLOT(filterNeedsInputParameters(AbstractFilter*)));

  connect(attributeMatrixName, SIGNAL(textEdited(const QString&)),
    this, SLOT(widgetChanged(const QString&)));

  attributeMatrixName->blockSignals(true);
  attributeMatrixName->clear();
  // Now let the gui send signals like normal
  attributeMatrixName->blockSignals(false);

  hideButton();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AttributeMatrixCreationWidget::checkStringValues(QString curDcName, QString filtDcName)
{
  if (curDcName.isEmpty() == true && filtDcName.isEmpty() == false)
  {
    return filtDcName;
  }
  else if (curDcName.isEmpty() == false && filtDcName.isEmpty() == true)
  {
    return curDcName;
  }
  else if (curDcName.isEmpty() == false && filtDcName.isEmpty() == false && m_DidCausePreflight == true)
  {
    return curDcName;
  }

  return filtDcName;

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::createSelectionMenu()
{
  // Now get the DataContainerArray from the Filter instance
  // We are going to use this to get all the current DataContainers
  DataContainerArray::Pointer dca = getFilter()->getDataContainerArray();
  if(NULL == dca.get()) { return; }

  // Get the menu and clear it out
  QMenu* menu = m_SelectedDataContainerPath->menu();
  if(!menu)
  {
    menu = new QMenu();
    m_SelectedDataContainerPath->setMenu(menu);
    menu->installEventFilter(this);
  }
  if(menu) {
    menu->clear();
  }

  // Get the DataContainerArray object
  // Loop over the data containers until we find the proper data container
  QList<DataContainer::Pointer> containers = dca->getDataContainers();
  QVector<unsigned int> geomTypes = m_FilterParameter->getDefaultGeometryTypes();

  QListIterator<DataContainer::Pointer> containerIter(containers);
  while(containerIter.hasNext())
  {
    DataContainer::Pointer dc = containerIter.next();

    IGeometry::Pointer geom = IGeometry::NullPointer();
    uint32_t geomType = 999;
    if (NULL != dc.get()) { geom = dc->getGeometry(); }
    if (NULL != geom.get()) { geomType = geom->getGeometryType(); }

    QString dcName = dc->getName();
    QAction* action = new QAction(dcName, menu);
    DataArrayPath dcPath(dcName, "", "");
    QString path = dcPath.serialize(Detail::Delimiter);
    action->setData(path);

    connect(action, SIGNAL(triggered(bool)), m_MenuMapper, SLOT(map()));
    m_MenuMapper->setMapping(action, path);
    menu->addAction(action);

    if(geomTypes.isEmpty() == false && geomTypes.contains(geomType) == false )
    {
      action->setDisabled(true);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool AttributeMatrixCreationWidget::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::Show && obj == m_SelectedDataContainerPath->menu())
  {
    QPoint pos = adjustedMenuPosition(m_SelectedDataContainerPath);
    m_SelectedDataContainerPath->menu()->move(pos);
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::dataContainerSelected(QString path)
{
  setSelectedPath(path);

  m_DidCausePreflight = true;
  emit parametersChanged();
  m_DidCausePreflight = false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::setSelectedPath(QString path)
{
  DataArrayPath amPath = DataArrayPath::Deserialize(path, Detail::Delimiter);
  if (amPath.isEmpty()) { return; }

  m_SelectedDataContainerPath->setText("");
  m_SelectedDataContainerPath->setToolTip("");
  attributeMatrixName->clear();

  DataContainerArray::Pointer dca = getFilter()->getDataContainerArray();
  if(NULL == dca.get()) { return; }

  DataContainer::Pointer dc = dca->getPrereqDataContainer<AbstractFilter>(getFilter(), amPath.getDataContainerName());
  if(nullptr != dc.get()) {
    QString html = dc->getInfoString(SIMPL::HtmlFormat);
    m_SelectedDataContainerPath->setToolTip(html);
    m_SelectedDataContainerPath->setText(amPath.getDataContainerName());
    attributeMatrixName->setText(amPath.getAttributeMatrixName());
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::on_attributeMatrixName_returnPressed()
{
  on_applyChangesBtn_clicked();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::hideButton()
{
  attributeMatrixName->setToolTip("");
  applyChangesBtn->setVisible(false);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::widgetChanged(const QString& text)
{
  attributeMatrixName->setStyleSheet(QString::fromLatin1("color: rgb(255, 0, 0);"));
  attributeMatrixName->setToolTip("Press the 'Return' key to apply your changes");
  if (applyChangesBtn->isVisible() == false)
  {
    applyChangesBtn->setVisible(true);
    fadeInWidget(applyChangesBtn);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::beforePreflight()
{
  if (NULL == getFilter()) { return; }
  if (m_DidCausePreflight == true)
  {
   // std::cout << "***  AttributeMatrixCreationWidget already caused a preflight, just returning" << std::endl;
    return;
  }

  DataArrayPath path = getFilter()->property(PROPERTY_NAME_AS_CHAR).value<DataArrayPath>();

  setSelectedPath(path.serialize(Detail::Delimiter));
  createSelectionMenu();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::afterPreflight()
{
  // std::cout << "After Preflight" << std::endl;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::filterNeedsInputParameters(AbstractFilter* filter)
{
  // Generate the path to the AttributeArray
  DataArrayPath selectedPath = DataArrayPath::Deserialize(m_SelectedDataContainerPath->text(), Detail::Delimiter);
  selectedPath.setAttributeMatrixName(attributeMatrixName->text());
  QVariant var;
  var.setValue(selectedPath);
  bool ok = false;
  // Set the value into the Filter
  ok = filter->setProperty(PROPERTY_NAME_AS_CHAR, var);
  if (false == ok)
  {
    FilterParameterWidgetsDialogs::ShowCouldNotSetFilterParameter(getFilter(), getFilterParameter());
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AttributeMatrixCreationWidget::on_applyChangesBtn_clicked()
{
  m_DidCausePreflight = true;

  attributeMatrixName->setStyleSheet(QString(""));
  emit parametersChanged();

  QPointer<QtSFaderWidget> faderWidget = new QtSFaderWidget(applyChangesBtn);
  setFaderWidget(faderWidget);

  if (getFaderWidget())
  {
    faderWidget->close();
  }
  faderWidget = new QtSFaderWidget(applyChangesBtn);
  faderWidget->setFadeOut();
  connect(faderWidget, SIGNAL(animationComplete()),
    this, SLOT(hideButton()));
  faderWidget->start();

  m_DidCausePreflight = false;
}