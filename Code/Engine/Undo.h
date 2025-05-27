#pragma once

#include "ECS.h"
#include "RTTI.h"
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
    UndoSystem(Scene& inScene) : m_Scene(inScene) {}

    template<typename T> requires std::is_base_of_v<UndoAction, T>
    void PushUndo(const T& inAction)
    {
        m_RedoStack.clear();
        m_UndoStack.push_back(std::make_shared<T>(inAction));
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

template<typename T> requires ( sizeof(T) < 1024 && std::default_initializable<T> )
    struct ComponentUndo : public UndoAction
{
    T current;
    T previous;
    Entity entity = Entity::Null;

    virtual const char* GetName() { return RTTI_OF<T>().GetTypeName(); }

    void Undo(Scene& inScene) override
    {
        T& component = inScene.Get<T>(entity);
        component = previous;
    }

    void Redo(Scene& inScene) override
    {
        T& component = inScene.Get<T>(entity);
        component = current;
    }
};

}