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

#include "DataContainerReader.h"

#include <QtCore/QFileInfo>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainerBundle.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/BooleanFilterParameter.h"
#include "SIMPLib/FilterParameters/DataContainerReaderFilterParameter.h"
#include "SIMPLib/FilterParameters/H5FilterParametersReader.h"
#include "SIMPLib/Filtering/FilterManager.h"
#include "SIMPLib/Utilities/SIMPLH5DataReader.h"
#include "SIMPLib/SIMPLibVersion.h"

// Include the MOC generated file for this class
#include "moc_DataContainerReader.cpp"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataContainerReader::DataContainerReader()
: AbstractFilter()
, m_InputFile("")
, m_OverwriteExistingDataContainers(false)
, m_LastFileRead("")
, m_LastRead(QDateTime::currentDateTime())
, m_InputFileDataContainerArrayProxy()
{
  m_PipelineFromFile = FilterPipeline::New();

  m_DataReader = new SIMPLH5DataReader();
  connect(m_DataReader, &SIMPLH5DataReader::errorGenerated, [=] (const QString &msg, const int &code) {
    setErrorCondition(code);
    notifyErrorMessage(getHumanLabel(), msg, getErrorCondition());
  });

  setupFilterParameters();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataContainerReader::~DataContainerReader()
{
  delete m_DataReader;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::setupFilterParameters()
{
  FilterParameterVector parameters;

  parameters.push_back(SIMPL_NEW_BOOL_FP("Overwrite Existing Data Containers", OverwriteExistingDataContainers, FilterParameter::Parameter, DataContainerReader));
  {
    DataContainerReaderFilterParameter::Pointer parameter = DataContainerReaderFilterParameter::New();
    parameter->setHumanLabel("Select Arrays from Input File");
    parameter->setPropertyName("InputFileDataContainerArrayProxy");
    parameter->setDefaultFlagValue(Qt::Checked);
    parameter->setInputFileProperty("InputFile");
    parameter->setCategory(FilterParameter::Parameter);
    parameter->setFilter(this);
    parameters.push_back(parameter);
  }

  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setInputFile(reader->readString("InputFile", getInputFile()));
  setInputFileDataContainerArrayProxy(reader->readDataContainerArrayProxy("InputFileDataContainerArrayProxy", getInputFileDataContainerArrayProxy()));
  syncProxies(); // Sync the file proxy and currently cached proxy together into one proxy
  setOverwriteExistingDataContainers(reader->readValue("OverwriteExistingDataContainers", getOverwriteExistingDataContainers()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::readFilterParameters(QJsonObject& obj)
{
  AbstractFilter::readFilterParameters(obj);
  syncProxies();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::preWriteFilterParameters(QJsonObject &obj, QJsonObject &rootObject)
{

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::writeFilterParameters(QJsonObject& obj)
{
  //writeExistingPipelineToFile(obj);
  AbstractFilter::writeFilterParameters(obj);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::initialize()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::dataCheck()
{  
  if (m_DataReader->openFile(getInputFile()) == false)
  {
    return;
  }

  // Sync the file proxy and cached proxy if the time stamps are different
  QFileInfo fi(getInputFile());
  if(getInputFile() == getLastFileRead() && getLastRead() < fi.lastModified())
  {
    if (syncProxies() == false)
    {
      return;
    }
  }

  QString ss;
  if(getInputFile().isEmpty() == true)
  {
    ss = QObject::tr("The input file must be set");
    setErrorCondition(-387);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
  }
  else if(fi.exists() == false)
  {
    ss = QObject::tr("The input file %1 does not exist").arg(getInputFile());
    setErrorCondition(-388);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
  }

  if(getErrorCondition())
  {
    // something has gone wrong and errors were logged alread so just return
    return;
  }

  DataContainerArray::Pointer dca = getDataContainerArray();

  // Create a new DataContainerArray to read the file data into
  DataContainerArray::Pointer tempDCA = DataContainerArray::New();

  // Read either the structure or all the data depending on the preflight status
  readData(m_InputFileDataContainerArrayProxy, tempDCA);

  QList<DataContainer::Pointer>& tempContainers = tempDCA->getDataContainers();
  QListIterator<DataContainer::Pointer> iter(tempContainers);
  while(iter.hasNext())
  {
    DataContainer::Pointer container = iter.next();

    if(getOverwriteExistingDataContainers() == true)
    {
      if(dca->doesDataContainerExist(container->getName()) == true)
      {
        dca->removeDataContainer(container->getName());
      }
      dca->addDataContainer(container);
    }
    else
    {
      if(dca->doesDataContainerExist(container->getName()) == true)
      {
        ss = QObject::tr("The input file has a DataContainer with a name (%1) that already exists in the current DataContainerArray structure").arg(container->getName());
        setErrorCondition(-390);
        notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
      }
      else
      {
        dca->addDataContainer(container);
      }
    }
  }
  QMap<QString, IDataContainerBundle::Pointer> bundles = tempDCA->getDataContainerBundles();
  dca->setDataContainerBundles(bundles);

  m_DataReader->closeFile();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::preflight()
{
  setInPreflight(true);
  // Annouce we are about to preflight
  // The GUI will pick up the structure
  emit preflightAboutToExecute();
  // The Gui sends down any changes to the Proxy (which for preflight we don't care about)
  emit updateFilterParameters(this);
  // to the read here because this will populate the DataContainerArray with our DataContainer
  dataCheck();
  // The GUI needs to send down the selections that were made by the user and we need to update
  // DataContainerArray->DataContainer object so the rest of the pipeline has the proper information
  emit preflightExecuted(); // Done executing
  setInPreflight(false);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::execute()
{
  /* In this VERY Special circumstance, the data check will actually read the data from the
   * file and move those DataContainer objects into the existing DataContainerArray. Error messages
   * will be passed up the chain if something goes wrong.
   */
  dataCheck();

  notifyStatusMessage(getHumanLabel(), "Complete");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::readData(DataContainerArrayProxy& proxy, DataContainerArray::Pointer dca)
{
  setErrorCondition(0);
  setWarningCondition(0);

  bool result = m_DataReader->readDREAM3DData(getInPreflight(), proxy, dca.get());
  if (result == false)
  {
    return;
  }

  hid_t fileId = QH5Utilities::openFile(getInputFile(), true); // Open the file Read Only
  if(fileId < 0)
  {
    setErrorCondition(-150);
    QString ss = QObject::tr("Error opening input file '%1'").arg(getInputFile());
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    return;
  }

  if(!getInPreflight())
  {
    int32_t err = readExistingPipelineFromFile(fileId);
    if(err < 0)
    {
      setErrorCondition(err);
      QString ss = QObject::tr("Error trying to read the existing pipeline from the file '%1'").arg(getInputFile());
      notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    }
  }

  QH5Utilities::closeFile(fileId);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataContainerArrayProxy DataContainerReader::readDataContainerArrayStructure(const QString& path)
{
  DataContainerArrayProxy proxy;
  QFileInfo fi(path);
  if(path.isEmpty() == true)
  {
    QString ss = QObject::tr("SIMPLView File Path is empty");
    setErrorCondition(-70);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    return proxy;
  }
  else if(fi.exists() == false)
  {
    QString ss = QObject::tr("The input file %1 does not exist").arg(path);
    setErrorCondition(-388);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    return proxy;
  }

  herr_t err = 0;
  hid_t fileId = QH5Utilities::openFile(path, true);
  if(fileId < 0)
  {
    QString ss = QObject::tr("Error opening SIMPLView file location at %1").arg(path);
    setErrorCondition(-71);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    return proxy;
  }
  HDF5ScopedFileSentinel sentinel(&fileId, false); // Make sure the file gets closed automatically if we return early

  // Check the DREAM3D File Version to make sure we are reading the proper version
  QString d3dVersion;
  err = QH5Lite::readStringAttribute(fileId, "/", SIMPL::HDF5::DREAM3DVersion, d3dVersion);
  if(err < 0)
  {
    QString ss = QObject::tr("HDF5 Attribute '%1' was not found on the HDF5 root node and this is required").arg(SIMPL::HDF5::DREAM3DVersion);
    setErrorCondition(-72);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    return proxy;
  }
  //  else {
  //    std::cout << SIMPL::HDF5::DREAM3DVersion.toStdString() << ":" << d3dVersion.toStdString() << std::endl;
  //  }

  QString fileVersion;
  err = QH5Lite::readStringAttribute(fileId, "/", SIMPL::HDF5::FileVersionName, fileVersion);
  if(err < 0)
  {
    // std::cout << "Attribute '" << SIMPL::HDF5::FileVersionName.toStdString() << " was not found" << std::endl;
    QString ss = QObject::tr("HDF5 Attribute '%1' was not found on the HDF5 root node and this is required").arg(SIMPL::HDF5::FileVersionName);
    setErrorCondition(-73);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    return proxy;
  }
  //  else {
  //    std::cout << SIMPL::HDF5::FileVersionName.toStdString() << ":" << fileVersion.toStdString() << std::endl;
  //  }

  hid_t dcArrayGroupId = H5Gopen(fileId, SIMPL::StringConstants::DataContainerGroupName.toLatin1().constData(), H5P_DEFAULT);
  if(dcArrayGroupId < 0)
  {
    QString ss = QObject::tr("Error opening HDF5 Group '%1'").arg(SIMPL::StringConstants::DataContainerGroupName);
    setErrorCondition(-74);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    return proxy;
  }
  sentinel.addGroupId(&dcArrayGroupId);

  QString h5InternalPath = QString("/") + SIMPL::StringConstants::DataContainerGroupName;

  // Read the entire structure of the file into the proxy
  DataContainer::ReadDataContainerStructure(dcArrayGroupId, proxy, h5InternalPath);

  return proxy;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int DataContainerReader::readExistingPipelineFromFile(hid_t fileId)
{
  herr_t err = 0;
  m_PipelineFromFile->clear();

  H5FilterParametersReader::Pointer reader = H5FilterParametersReader::New();
  // First try to see if we can read the newer JSON style of Pipeline from the HDF5 based file
  m_PipelineFromFile = reader->readPipelineFromFile(fileId, nullptr);
  if(nullptr != m_PipelineFromFile.get())
  {
    return err;
  }

  // If we are on the old style then the shared pointer would have come back null so create a new one.
  m_PipelineFromFile = FilterPipeline::New();

  // HDF5: Open the "Pipeline" Group
  hid_t pipelineGroupId = H5Gopen(fileId, SIMPL::StringConstants::PipelineGroupName.toLatin1().data(), H5P_DEFAULT);
  reader->setPipelineGroupId(pipelineGroupId);

  // Use QH5Lite to ask how many "groups" are in the "Pipeline Group"
  QList<QString> groupList;
  err = QH5Utilities::getGroupObjects(pipelineGroupId, H5Utilities::H5Support_GROUP, groupList);

  // Loop over the items getting the "ClassName" attribute from each group
  QString classNameStr = "";
  for(int32_t i = 0; i < groupList.size(); i++)
  {
    QString ss = QString::number(i, 10);

    err = QH5Lite::readStringAttribute(pipelineGroupId, ss, "ClassName", classNameStr);
    if(err < 0)
    {
      qDebug() << "Filter Index = " << i;
    }
    // Instantiate a new filter using the FilterFactory based on the value of the className attribute
    FilterManager* fm = FilterManager::Instance();
    IFilterFactory::Pointer ff = fm->getFactoryForFilter(classNameStr);
    if(nullptr != ff.get())
    {
      AbstractFilter::Pointer filter = ff->create();
      if(nullptr != filter.get())
      {
        // Read the parameters
        filter->readFilterParameters(reader.get(), i);

        // Add filter to m_PipelineFromFile
        m_PipelineFromFile->pushBack(filter);
      }
    }
  }
  err = H5Gclose(pipelineGroupId);
  return err;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int DataContainerReader::writeExistingPipelineToFile(QJsonObject& rootJson, int index)
{
  FilterPipeline::FilterContainerType container = m_PipelineFromFile->getFilterContainer();
  int offset = 0;

  for(FilterPipeline::FilterContainerType::iterator iter = container.begin(); iter != container.end(); ++iter)
  {
    AbstractFilter::Pointer filter = *iter;
    QJsonObject json;

    QString numStr = QString::number(index++);

    if(nullptr != filter.get())
    {
      json[SIMPL::Settings::FilterName] = filter->getNameOfClass();
      json[SIMPL::Settings::HumanLabel] = filter->getHumanLabel();
      json[SIMPL::Settings::FilterEnabled] = filter->getEnabled();

      filter->writeFilterParameters(json);
    }
    else
    {
      json["Unknown Filter"] = "ERROR: Filter instance was nullptr within the SVPipelineFilterWidget instance. Report this error to the DREAM3D Developers";
    }

    rootJson[numStr] = json;
    offset++;
  }
  return offset;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DataContainerReader::syncProxies()
{
  // If there is something in the cached proxy...
  if(m_InputFileDataContainerArrayProxy.dataContainers.size() > 0)
  {
    DataContainerArrayProxy fileProxy;
    if (m_DataReader->readDataContainerArrayStructure(fileProxy) == false)
    {
      return false;
    }

    DataContainerArrayProxy cacheProxy = getInputFileDataContainerArrayProxy();

    // Mesh proxies together into one proxy
    DataContainerArrayProxy mergedProxy = DataContainerArrayProxy::MergeProxies(fileProxy, cacheProxy);
    setInputFileDataContainerArrayProxy(mergedProxy);
  }
  else
  {
    DataContainerArrayProxy fileProxy;
    if (m_DataReader->readDataContainerArrayStructure(fileProxy) == false)
    {
      return false;
    }

    setInputFileDataContainerArrayProxy(fileProxy);
  }

  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer DataContainerReader::newFilterInstance(bool copyFilterParameters)
{
  DataContainerReader::Pointer filter = DataContainerReader::New();
  if(true == copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());

    SIMPL_COPY_INSTANCEVAR(InputFile)

    filter->setInputFile(getInputFile());
#if 0
    filter->setOverwriteExistingDataContainers(getOverwriteExistingDataContainers());
    filter->setDataContainerArrayProxy(getDataContainerArrayProxy());
#endif
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString DataContainerReader::getCompiledLibraryName()
{
  return Core::CoreBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString DataContainerReader::getBrandingString()
{
  return "SIMPLib Core Filter";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString DataContainerReader::getFilterVersion()
{
  QString version;
  QTextStream vStream(&version);
  vStream << SIMPLib::Version::Major() << "." << SIMPLib::Version::Minor() << "." << SIMPLib::Version::Patch();
  return version;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString DataContainerReader::getGroupName()
{
  return SIMPL::FilterGroups::IOFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString DataContainerReader::getSubGroupName()
{
  return SIMPL::FilterSubGroups::InputFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString DataContainerReader::getHumanLabel()
{
  return "Read DREAM.3D Data File";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataContainerReader::cleanupFilter()
{
  m_PipelineFromFile->clear();
}
