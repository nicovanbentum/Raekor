#pragma once

#include "../Engine/Raekor.h"
#include "../Engine/Profiler.h"

namespace RK {

class MeshNodes
{
public:
    enum class Node : uint32_t
    {
        Null = UINT32_MAX
    };

    void Init(Scene* inScene, IRenderInterface* inRenderInterface, Entity inParent)
    {
        Clear();

        m_Scene = inScene;
        m_RenderInterface = inRenderInterface;
        m_Parent = inParent;
    }

    void Clear()
    {
        m_Nodes.clear();
        m_Batches.clear();
        m_BatchComponents.clear();
    }

    Node Create(Node inParent = Node::Null)
    {
        m_Nodes.emplace_back().mParent = inParent;
        return Node(m_Nodes.size() - 1);
    }

    Node CreateMesh(const Mesh& inMesh, Entity inMaterial, Node inParent = Node::Null)
    {
        const Node node = Create(inParent);
        m_Nodes[uint32_t(node)].mMaterial = inMaterial;
        SetMesh(node, &inMesh);
        return node;
    }

    void SetMesh(Node inNode, const Mesh* inMesh)
    {
        if (inNode == Node::Null)
            return;

        const uint32_t batch = inMesh ? GetBatch(*inMesh, m_Nodes[uint32_t(inNode)].mMaterial) : UINT32_MAX;
        NodeData& node = m_Nodes[uint32_t(inNode)];

        if (node.mBatch == batch)
            return;

        ReleaseInstance(node);

        if (batch == UINT32_MAX)
            return;

        Batch& target = m_Batches[batch];
        MeshInstanced& mesh_instanced = m_Scene->Get<MeshInstanced>(target.mEntity);

        if (target.mFreeInstances.empty())
        {
            node.mInstance = uint32_t(mesh_instanced.instances.size());
            mesh_instanced.instances.emplace_back();
        }
        else
        {
            node.mInstance = target.mFreeInstances.back();
            target.mFreeInstances.pop_back();
        }

        MeshInstanced::Instance& instance = mesh_instanced.instances[node.mInstance];
        instance.material = node.mMaterial;
        instance.visible = false;

        node.mBatch = batch;
    }

    void SetTransform(Node inNode, const Vec3& inPosition, const Vec3& inScale = Vec3(1.0f), const Quat& inRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f))
    {
        if (inNode == Node::Null)
            return;

        NodeData& node = m_Nodes[uint32_t(inNode)];
        node.mLocal = glm::scale(glm::translate(Mat4x4(1.0f), inPosition) * glm::toMat4(inRotation), inScale);
        node.mHidden = false;
    }

    void SetTransform(Node inNode, const Mat4x4& inLocal)
    {
        if (inNode == Node::Null)
            return;

        NodeData& node = m_Nodes[uint32_t(inNode)];
        node.mLocal = inLocal;
        node.mHidden = false;
    }

    void Park(Node inNode)
    {
        if (inNode != Node::Null)
            m_Nodes[uint32_t(inNode)].mHidden = true;
    }

    void SetMaterial(Node inNode, Entity inMaterial)
    {
        if (inNode != Node::Null)
            m_Nodes[uint32_t(inNode)].mMaterial = inMaterial;
    }

    void Update()
    {
        PROFILE_FUNCTION_CPU();

        m_BatchComponents.resize(m_Batches.size());

        for (size_t batch = 0; batch < m_Batches.size(); batch++)
            m_BatchComponents[batch] = m_Scene->GetPtr<MeshInstanced>(m_Batches[batch].mEntity);

        for (NodeData& node : m_Nodes)
        {
            if (node.mParent == Node::Null)
            {
                node.mWorld = node.mLocal;
                node.mVisible = !node.mHidden;
            }
            else
            {
                const NodeData& parent = m_Nodes[uint32_t(node.mParent)];
                node.mWorld = parent.mWorld * node.mLocal;
                node.mVisible = parent.mVisible && !node.mHidden;
            }

            if (node.mBatch == UINT32_MAX || m_BatchComponents[node.mBatch] == nullptr)
                continue;

            MeshInstanced::Instance& instance = m_BatchComponents[node.mBatch]->instances[node.mInstance];
            instance.transform = node.mWorld;
            instance.material = node.mMaterial;
            instance.visible = node.mVisible;
        }
    }

private:
    struct NodeData
    {
        Node mParent = Node::Null;
        uint32_t mBatch = UINT32_MAX;
        uint32_t mInstance = 0;
        Entity mMaterial = Entity::Null;
        Mat4x4 mLocal = Mat4x4(1.0f);
        Mat4x4 mWorld = Mat4x4(1.0f);
        bool mHidden = false;
        bool mVisible = false;
    };

    struct Batch
    {
        Entity mEntity = Entity::Null;
        const MeshGeometry* mGeometry = nullptr;
        Array<uint32_t> mFreeInstances;
    };

    uint32_t GetBatch(const Mesh& inMesh, Entity inMaterial)
    {
        const MeshGeometry* geometry = inMesh.GetGeometry();

        if (geometry == nullptr)
            return UINT32_MAX;

        for (uint32_t batch = 0; batch < m_Batches.size(); batch++)
        {
            if (m_Batches[batch].mGeometry == geometry)
                return batch;
        }

        const Entity entity = m_Scene->CreateSpatialEntity(geometry->name);
        m_Scene->ParentTo(entity, m_Parent);

        MeshInstanced& mesh_instanced = m_Scene->Add<MeshInstanced>(entity);
        mesh_instanced.SetAsset(inMesh.asset, inMesh.index);
        mesh_instanced.material = inMaterial;

        m_RenderInterface->UploadMeshBuffers(*mesh_instanced.asset);

        m_Batches.push_back(Batch { .mEntity = entity, .mGeometry = geometry });
        return uint32_t(m_Batches.size() - 1);
    }

    void ReleaseInstance(NodeData& ioNode)
    {
        if (ioNode.mBatch == UINT32_MAX)
            return;

        Batch& batch = m_Batches[ioNode.mBatch];

        if (MeshInstanced* mesh_instanced = m_Scene->GetPtr<MeshInstanced>(batch.mEntity))
            mesh_instanced->instances[ioNode.mInstance].visible = false;

        batch.mFreeInstances.push_back(ioNode.mInstance);
        ioNode.mBatch = UINT32_MAX;
    }

    Scene* m_Scene = nullptr;
    IRenderInterface* m_RenderInterface = nullptr;
    Entity m_Parent = Entity::Null;
    Array<NodeData> m_Nodes;
    Array<Batch> m_Batches;
    Array<MeshInstanced*> m_BatchComponents;
};

} // namespace RK
