#include "PCH.h"
#include "Undo.h"

namespace RK {

void UndoSystem::Undo()
{
	if (!HasUndo())
		return;

	//std::cout << "Undo " << m_UndoStack.back()->GetName() << " at index " << m_UndoStack.size() - 1 << std::endl;

	m_UndoStack.back()->Undo(m_Scene);
	m_RedoStack.push_back(m_UndoStack.back());
	m_UndoStack.pop_back();
}

void UndoSystem::Redo()
{
	if (!HasRedo())
		return;

	//std::cout << "Redo " << m_RedoStack.back()->GetName() << " at index " << m_RedoStack.size() - 1 << std::endl;

	m_RedoStack.back()->Redo(m_Scene);
	m_UndoStack.push_back(m_RedoStack.back());
	m_RedoStack.pop_back();
}

void UndoSystem::Clear()
{
	m_UndoStack.clear();
	m_RedoStack.clear();
}

}