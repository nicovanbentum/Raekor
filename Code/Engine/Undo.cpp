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

void TransformUndoAction::Undo(Scene& inScene)
{
	Transform& transform = inScene.Get<Transform>(entity);
	transform.localTransform = previous;
	transform.Decompose();
}

void TransformUndoAction::Redo(Scene& inScene) 
{
	Transform& transform = inScene.Get<Transform>(entity);
	transform.localTransform = current;
	transform.Decompose();
}

void MaterialUndoAction::Undo(Scene& inScene)
{
	Material& material = inScene.Get<Material>(entity);
	material = previous;
}

void MaterialUndoAction::Redo(Scene& inScene) 
{
	Material& material = inScene.Get<Material>(entity);
	material = current;
}

void CameraUndoAction::Undo(Scene& inScene)
{
	Camera& camera = inScene.Get<Camera>(entity);
	camera = previous;
}

void CameraUndoAction::Redo(Scene& inScene)
{
	Camera& camera = inScene.Get<Camera>(entity);
	camera = current;
}

}