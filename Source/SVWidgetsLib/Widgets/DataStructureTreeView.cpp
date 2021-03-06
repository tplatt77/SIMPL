/* ============================================================================
* Copyright (c) 2009-2019 BlueQuartz Software, LLC
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
*    United States Air Force Prime Contract FA8650-15-D-5280
*    United States Prime Contract Navy N00173-07-C-2068
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include "DataStructureTreeView.h"

#include <iostream>

#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QtGui/QMouseEvent>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>

#include "SVWidgetsLib/Core/SVWidgetsLibConstants.h"
#include "SVWidgetsLib/Widgets/DataArrayPathSelectionWidget.h"
#include "SVWidgetsLib/Widgets/DataStructureItemDelegate.h"
#include "SVWidgetsLib/Widgets/DataStructureProxyModel.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataStructureTreeView::DataStructureTreeView(QWidget* parent)
: QTreeView(parent)
{
  setAcceptDrops(true);
  setMouseTracking(true);
  setAttribute(Qt::WA_MacShowFocusRect, false);
  // model
  QStandardItemModel* model = new QStandardItemModel(this);
  model->setColumnCount(1);
  DataStructureProxyModel* proxyModel = new DataStructureProxyModel(this);
  proxyModel->setSourceModel(model);
  setModel(proxyModel);
  
  m_Delegate = new DataStructureItemDelegate(this);
  setItemDelegate(m_Delegate);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataStructureTreeView::~DataStructureTreeView() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QStandardItemModel* DataStructureTreeView::getStandardModel()
{
  return dynamic_cast<QStandardItemModel*>(getProxyModel()->sourceModel());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataStructureProxyModel* DataStructureTreeView::getProxyModel()
{
  return dynamic_cast<DataStructureProxyModel*>(model());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::setActiveFilter(AbstractFilter::Pointer filter)
{
  m_Filter = filter;
  m_Delegate->setActiveFilter(filter);
  viewport()->repaint();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::setViewRequirements(DataContainerSelectionFilterParameter::RequirementType reqs)
{
  m_Delegate->setViewRequirements(reqs);
  viewport()->repaint();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::setViewRequirements(AttributeMatrixSelectionFilterParameter::RequirementType reqs)
{
  m_Delegate->setViewRequirements(reqs);
  viewport()->repaint();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::setViewRequirements(DataArraySelectionFilterParameter::RequirementType reqs)
{
  m_Delegate->setViewRequirements(reqs);
  viewport()->repaint();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::clearViewRequirements()
{
  m_Delegate->clearRequirements();
  viewport()->repaint();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataArrayPath DataStructureTreeView::getDataArrayPath(const QModelIndex& index)
{
  DataArrayPath path;
  DataStructureProxyModel* proxyModel = dynamic_cast<DataStructureProxyModel*>(getProxyModel());
  QStandardItemModel* stdModel = dynamic_cast<QStandardItemModel*>(getStandardModel());
  
  if(nullptr == proxyModel || nullptr == stdModel)
  {
    return path;
  }

  if(index.isValid())
  {
    QStandardItem* item = stdModel->itemFromIndex(proxyModel->mapToSource(index));
    if(item->parent() != nullptr)
    {
      QStandardItem* parentItem = item->parent();
      if(parentItem->parent() != nullptr)
      {
        path.setDataContainerName(parentItem->parent()->text());
        path.setAttributeMatrixName(parentItem->text());
        path.setDataArrayName(item->text());
      }
      else
      {
        path.setDataContainerName(parentItem->text());
        path.setAttributeMatrixName(item->text());
      }
    }
    else
    {
      path.setDataContainerName(item->text());
    }
  }

  return path;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::emitFilterPath(QModelIndex& index)
{
  // Do not filter the FilterInputWidget while the view is being filtered
  if(m_Delegate->isFiltered())
  {
    return;
  }

  if(!index.isValid())
  {
    emit endDataStructureFiltering();
    return;
  }

  DataArrayPath path = getDataArrayPath(index);
  emit filterPath(path);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::mousePressEvent(QMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
  {
    m_StartPos = event->pos();
  }

  QTreeView::mousePressEvent(event);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::mouseMoveEvent(QMouseEvent* event)
{
  QTreeView::mouseMoveEvent(event);

  if(event->buttons() & static_cast<int>(Qt::LeftButton != 0u != 0u))
  {
    QModelIndex index = indexAt(m_StartPos);
    if(m_Delegate->isFiltered() && !m_Delegate->indexMatchesReqs(index))
    {
      return;
    }

    int distance = (event->pos() - m_StartPos).manhattanLength();
    if(distance >= QApplication::startDragDistance() && index.isValid())
    {
      performDrag();
    }
  }
  else
  {
    QModelIndex index = indexAt(event->pos());
    if(m_Delegate->isFiltered() && !m_Delegate->indexMatchesReqs(index))
    {
      return;
    }

    emitFilterPath(index);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::leaveEvent(QEvent* event)
{
  QTreeView::leaveEvent(event);

  if(m_Dragging)
  {
    return;
  }

  emit endDataStructureFiltering();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::performDrag()
{
  QModelIndex index = indexAt(m_StartPos);
  if(!index.isValid())
  {
    return;
  }

  DataArrayPath path = getDataArrayPath(index);

  // Do not allow dragging paths created by the current filter
  std::list<DataArrayPath> createdPaths = m_Filter->getCreatedPaths();
  if(std::find(createdPaths.begin(), createdPaths.end(), path) != createdPaths.end())
  {
    return;
  }

  QMimeData* mimeData = new QMimeData;
  mimeData->setData(SIMPLView::DragAndDrop::DataArrayPath, path.serialize().toUtf8());
  QPixmap dragIcon = DataArrayPathSelectionWidget::CreateDragIcon(path);

  m_Dragging = true;
  QDrag* drag = new QDrag(this);
  drag->setMimeData(mimeData);
  drag->setPixmap(dragIcon);
  drag->exec(Qt::CopyAction);

  // drag->exec is a blocking method
  dragComplete();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::dragComplete()
{
  emit endDataStructureFiltering();
  m_Dragging = false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::dragEnterEvent(QDragEnterEvent* event)
{
  if(event->mimeData()->hasFormat(SIMPLView::DragAndDrop::SelectionWidget))
  {
    DataArrayPathSelectionWidget* selectionWidget = dynamic_cast<DataArrayPathSelectionWidget*>(event->source());
    if(selectionWidget != nullptr)
    {
      event->accept();
      return;
    }
  }

  event->ignore();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::dragMoveEvent(QDragMoveEvent* event)
{
  // If Source is DataArrayPathSelectionWidget
  if(event->mimeData()->hasFormat(SIMPLView::DragAndDrop::SelectionWidget))
  {
    DataArrayPathSelectionWidget* selectionWidget = dynamic_cast<DataArrayPathSelectionWidget*>(event->source());
    if(selectionWidget != nullptr)
    {
      // Get the DataArrayPath under the cursor
      QModelIndex index = indexAt(event->pos());
      if(!index.isValid())
      {
        return;
      }
      DataArrayPath path = getDataArrayPath(index);

      // Check path requirements
      if(selectionWidget->checkPathReqs(path))
      {
        event->accept();
        return;
      }
    }
    // End SelectionWidget
  }

  event->ignore();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::dropEvent(QDropEvent* event)
{
  // If Source is DataArrayPathSelectionWidget
  if(event->mimeData()->hasFormat(SIMPLView::DragAndDrop::SelectionWidget))
  {
    DataArrayPathSelectionWidget* selectionWidget = dynamic_cast<DataArrayPathSelectionWidget*>(event->source());
    if(selectionWidget != nullptr)
    {
      QModelIndex index = indexAt(event->pos());
      if(!index.isValid())
      {
        return;
      }
      DataArrayPath path = getDataArrayPath(index);

      // Check path requirements
      if(selectionWidget->checkPathReqs(path))
      {
        selectionWidget->setDataArrayPath(path);
        event->accept();
        return;
      }
    }
  }

  event->ignore();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::collapseAllBut(const QModelIndex& index)
{
  QVector<QModelIndex> expandedChildren = getExpandedChildren(index);
  if(isExpanded(index))
  {
    expandedChildren.push_front(index);
  }
  collapseAll();

  // Disable Animation
  bool prevAnimationState = isAnimated();
  setAnimated(false);
  for(const QModelIndex& child : expandedChildren)
  {
    expand(child);
  }
  QModelIndex parentIndex = index.parent();
  while(parentIndex.isValid())
  {
    expand(parentIndex);
    parentIndex = parentIndex.parent();
  }
  // Reset animation
  setAnimated(prevAnimationState);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::findExpandedChildren(QAbstractItemModel* model, const QModelIndex& index, QVector<QModelIndex>& expandedVector)
{
  if(isExpanded(index))
  {
    expandedVector.push_back(index);
  }
  const int childCount = model->rowCount(index);
  for(int i = 0; i < childCount; i++)
  {
    QModelIndex childIndex = model->index(i, 0, index);
    if(isExpanded(childIndex))
    {
      expandedVector.push_back(childIndex);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QVector<QModelIndex> DataStructureTreeView::getExpandedChildren(const QModelIndex& index)
{
  QVector<QModelIndex> expandedChildren;
  QAbstractItemModel* abstrModel = model();
  const int childCount = abstrModel->rowCount(index);
  for(int i = 0; i < childCount; i++)
  {
    QModelIndex childIndex = abstrModel->index(i, 0, index);
    findExpandedChildren(abstrModel, childIndex, expandedChildren);
  }

  return expandedChildren;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
  QModelIndex index = indexAt(event->pos());
  if(!index.isValid())
  {
    return;
  }

  DataArrayPath path = getDataArrayPath(index);
  if(m_Delegate->pathMatchesReqs(path))
  {
    emit applyPathToFilteringParameter(path);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::contextMenuEvent(QContextMenuEvent* event)
{
  const QPoint pos = event->pos();
  const QModelIndex index = indexAt(pos);
  DataStructureProxyModel* proxyModel = getProxyModel();
  const bool isValid = index.isValid();
  const int numChildren = proxyModel->rowCount(index);

  QMenu menu;
  QAction* expandAllAction = menu.addAction("Expand All");
  QAction* collapseAllAction = menu.addAction("Collapse All");

  connect(expandAllAction, &QAction::triggered, this, &DataStructureTreeView::expandAll);
  connect(collapseAllAction, &QAction::triggered, this, &DataStructureTreeView::collapseAll);

  if(isValid && numChildren > 0)
  {
    menu.addSeparator();
    QAction* collapseAllButAction = menu.addAction("Collapse All But This");
    connect(collapseAllButAction, &QAction::triggered, [=]{
      collapseAllBut(index);
    });
  }

  menu.exec(event->globalPos());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::search(const QString& name)
{
  if(name.isEmpty())
  {
    getProxyModel()->setFilterRegExp(QRegExp());
  }
  else
  {
    getProxyModel()->setFilterRegExp(QRegExp(name, Qt::CaseInsensitive));
  }
  update();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DataStructureTreeView::rowsInserted(const QModelIndex& parent, int start, int end)
{
  QTreeView::rowsInserted(parent, start, end);

  // Expand if the specified parent did not have children previously
  if(0 == start && !parent.child(end + 1, 0).isValid())
  {
    expand(parent);
  }
}
