#pragma once

#include "ECS.h"
#include "Scene.h"
#include "Components.h"

namespace RK {

struct UndoAction
{
	virtual ~UndoAction() = default;

	virtual const char* GetName() = 0;
	virtual void Undo(Scene& inScene) = 0;
	virtual void Redo(Scene& inScene) = 0;
};

class UndoSystem
{
public:
	UndoSystem(Scene& inScene) : m_Scene(inScene) { }

	template<typename taType>
	void PushAction(const taType& inAction) 
	{ 
		m_RedoStack.clear();
		m_UndoStack.push_back(std::make_shared<taType>(inAction)); 
		//std::cout << "Pushed " << m_UndoStack.back()->GetName() << " at index " << m_UndoStack.size() - 1 << std::endl;
	}

	bool HasUndo() const { return !m_UndoStack.empty(); }
	bool HasRedo() const { return !m_RedoStack.empty(); }

	void Undo();
	void Redo();
	void Clear();

private:
	Scene& m_Scene;
	Array<SharedPtr<UndoAction>> m_UndoStack;
	Array<SharedPtr<UndoAction>> m_RedoStack;
};

struct TransformUndoAction : public UndoAction
{
	Entity entity = Entity::Null;
	Mat4x4 current;
	Mat4x4 previous;
	
	virtual const char* GetName() { return "Transform"; }

	void Undo(Scene& inScene) override;
	void Redo(Scene& inScene) override;
};

struct MaterialUndoAction : public UndoAction
{
	Entity entity = Entity::Null;
	Material current;
	Material previous;

	virtual const char* GetName() { return "Material"; }

	void Undo(Scene& inScene) override;
	void Redo(Scene& inScene) override;
};

struct CameraUndoAction : public UndoAction
{
	Entity entity = Entity::Null;
	Camera current;
	Camera previous;

	virtual const char* GetName() { return "Material"; }

	void Undo(Scene& inScene) override;
	void Redo(Scene& inScene) override;
};

}