#include "pch.h"
#include "inspectorWidget.h"

#include "OS.h"
#include "GUI.h"
#include "JSON.h"
#include "Iter.h"
#include "Scene.h"
#include "Script.h"
#include "Archive.h"
#include "Physics.h"
#include "Animation.h"
#include "Components.h"
#include "Application.h"
#include "DebugRenderer.h"

#include "ViewportWidget.h"
#include "SequenceWidget.h"
#include "NodeGraphWidget.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(InspectorWidget) {}


InspectorWidget::InspectorWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_INFO_CIRCLE " Inspector " )) {}


void InspectorWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();

	ViewportWidget* viewport_widget = inWidgets->GetWidget<ViewportWidget>();
	SequenceWidget* sequence_widget = inWidgets->GetWidget<SequenceWidget>();
	ShaderGraphWidget* nodegraph_widget = inWidgets->GetWidget<ShaderGraphWidget>();

	m_SceneChanged = false;

	if (viewport_widget && GetActiveEntity() != Entity::Null)
	{
		m_SceneChanged = DrawEntityInspector(inWidgets);
	}
    else if (sequence_widget && sequence_widget->IsVisible() && sequence_widget->GetSelectedKeyFrame() != -1)
    {
        DrawKeyFrameInspector(inWidgets);
    }
	
	ImGui::End();
};


bool InspectorWidget::DrawEntityInspector(Widgets* inWidgets)
{
	Entity active_entity = m_Editor->GetActiveEntity();
	if (active_entity == Entity::Null)
		return false;

	ImGui::Text("Entity ID: %i", active_entity);

	if (GetScene().HasParent(active_entity))
	{
		ImGui::Text("Parent ID:");
		ImGui::SameLine();
	
		Entity parent = GetScene().GetParent(active_entity);
		String button_text = std::format("{}", uint32_t(parent));

		if (ImGui::SmallButton(button_text.c_str()))
			SetActiveEntity(parent);
	}

	Scene& scene = GetScene();
	Assets& assets = GetAssets();
	bool scene_changed = false;

	// I much prefered the for_each_tuple_element syntax tbh
	std::apply([&](const auto& ... components)
	{
		( ..., [&](Assets& assets, Scene& scene, Entity& entity)
		{
			using ComponentType = typename std::decay<decltype( components )>::type::type;

			if (scene.Has<ComponentType>(entity))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				
				bool is_open = true;
				if (ImGui::CollapsingHeader(components.name, ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PopStyleVar();

					if (is_open) 
						scene_changed |= DrawComponent(entity, scene.Get<ComponentType>(entity));
				}
				else
					ImGui::PopStyleVar();
			}

		}( assets, scene, active_entity ) );
	}, Components);

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);

	if (ImGui::BeginPopup("Components"))
	{
		for (const RTTI* rtti : g_RTTIFactory)
		{
			if (!rtti->IsDerivedFrom(&RTTI_OF(SceneComponent)))
				continue;

			if (scene.Has(active_entity, rtti))
				continue;

			if (ImGui::Selectable(rtti->GetTypeName(), false))
			{
				scene.Add(active_entity, rtti);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	return scene_changed;
}


void InspectorWidget::DrawJSONInspector(Widgets* inWidgets)
{
	ImGui::Text("JSON Inspector");
}


void InspectorWidget::DrawKeyFrameInspector(Widgets* inWidgets)
{
    SequenceWidget* sequencer_widget = inWidgets->GetWidget<SequenceWidget>();
    
    const int selected_key_frame = sequencer_widget->GetSelectedKeyFrame();

    if (selected_key_frame != -1)
    {
        CameraSequence::KeyFrame& key_frame = sequencer_widget->GetSequence().GetKeyFrame(selected_key_frame);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				
        if (ImGui::CollapsingHeader("KeyFrame Properties", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PopStyleVar();

            ImGui::SetNextItemRightAlign("Time  ");
            ImGui::SliderFloat("##keyframetime", &key_frame.mTime, 0.0f, sequencer_widget->GetSequence().GetDuration());

            ImGui::SetNextItemRightAlign("Angle  ");
            ImGui::DragFloat2("##keyframeangle", glm::value_ptr(key_frame.mAngle), 0.01f, -M_PI, M_PI, "%.2f");

            ImGui::SetNextItemRightAlign("Position ");
            ImGui::DragVec3("##keyframepos", key_frame.mPosition, 0.001f, -FLT_MAX, FLT_MAX);
        }
        else
            ImGui::PopStyleVar();
    }
}


void InspectorWidget::DrawScriptMember(const char* inLabel, int& ioValue) { ImGui::DragInt(inLabel, &ioValue); }


void InspectorWidget::DrawScriptMember(const char* inLabel, bool& ioValue) { ImGui::Checkbox(inLabel, &ioValue);  }


void InspectorWidget::DrawScriptMember(const char* inLabel, float& ioValue) 
{ 
	ImGui::DragFloat(inLabel, &ioValue, 0.001f, FLT_MIN, FLT_MAX, "%.3f");
}


void InspectorWidget::DrawScriptMember(const char* inLabel, Entity& ioValue)
{
	
	if (ioValue == Entity::Null)
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

		ImGui::DragDropTargetButton(inLabel, "None", false);

		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));

		bool clicked = false;

		if (Name* name = GetScene().GetPtr<Name>(ioValue))
		{
			clicked |= ImGui::DragDropTargetButton(inLabel, name->name.c_str(), true);
		}
		else 
		{
			std::string uint_value = std::to_string(ioValue);
			clicked |= ImGui::DragDropTargetButton(inLabel, uint_value.c_str(), true);
		}

		if (clicked)
			SetActiveEntity(ioValue);

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Clear"))
				ioValue = Entity::Null;

			ImGui::EndPopup();
		}

		ImGui::PopStyleColor();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
			ioValue = *reinterpret_cast<const Entity*>( payload->Data );

		ImGui::EndDragDropTarget();
	}
}


bool InspectorWidget::DrawComponent(Entity inEntity, Name& inName)
{
	if (ImGui::BeginTable("##NameTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableNextColumn();

		ImGui::AlignTextToFramePadding();

		ImGui::Text("Name");

		ImGui::TableNextColumn();

		ImGui::InputText("##NameInputText", &inName.name, ImGuiInputTextFlags_AutoSelectAll);

		ImGui::EndTable();
	}

	return false;
}


bool InspectorWidget::DrawComponent(Entity inEntity, Mesh& ioMesh)
{
	const int byte_size = sizeof(Mesh) + ioMesh.GetVertexStride() * ioMesh.positions.size();
	ImGui::Text("%.1f Kb", float(byte_size) / 1024);

	ImGui::Text("%i Meshlets", ioMesh.meshlets.size());
	ImGui::Text("%i Vertices", ioMesh.positions.size());
	ImGui::Text("%i Triangles", ioMesh.indices.size() / 3);

	Scene& scene = GetScene();
	bool scene_changed = false;

	if (scene.Has<Material>(ioMesh.material) && scene.Has<Material, Name>(ioMesh.material))
	{
		const auto& [material, name] = scene.Get<Material, Name>(ioMesh.material);

		const uint64_t albedo_imgui_id = m_Editor->GetRenderInterface()->GetImGuiTextureID(material.gpuAlbedoMap);
		const ImVec4 tint_color = ImVec4(material.albedo.r, material.albedo.g, material.albedo.b, material.albedo.a);

		if (ImGui::DragDropTargetButton("Material", name.name.c_str(), true))
			SetActiveEntity(ioMesh.material);

		if (ImGui::BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Clear")) 
			{
				scene_changed = true;
				ioMesh.material = Entity::Null;
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
			{
				scene_changed = true;
				ioMesh.material = *reinterpret_cast<const Entity*>( payload->Data );
			}

			ImGui::EndDragDropTarget();
		}

		//ImGui::SameLine();

		////if (ImGui::ImageButton((void*)(intptr_t)albedo_imgui_id, preview_size))
		////	SetActiveEntity(ioMesh.material);

		////ImGui::SameLine();
		//
		//const auto label_size = ImGui::CalcTextSize("Material");
		//const auto button_size = ImVec2(ImGui::GetFontSize() * ImGui::GetWindowDpiScale(), ImGui::GetFontSize() * ImGui::GetWindowDpiScale());

		//ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x - (button_size.x*2) - label_size.x - ImGui::GetStyle().FramePadding.x, ImGui::GetCursorPos().y));

		//ImGui::Button((const char*)ICON_FA_DOT_CIRCLE);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

		ImGui::DragDropTargetButton("Material", "None", false);

		ImGui::PopStyleColor();
		
		if (ImGui::BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft))
		{
			if (ImGui::MenuItem("Create"))
			{
				ioMesh.material = GetScene().Create();

				Name& name = GetScene().Add<Name>(ioMesh.material);
				Material& material = GetScene().Add<Material>(ioMesh.material);

				name.name = "NewMaterial";
				material = Material::Default;

				scene_changed = true;
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
			{
				scene_changed = true;
				ioMesh.material = *reinterpret_cast<const Entity*>( payload->Data );
			}

			ImGui::EndDragDropTarget();
		}
	}

	ImGui::DragFloat("LOD Fade", &ioMesh.lodFade, 0.001f, -1.0f, 1.0f, "%.3f");

	if (ImGui::Button("Generate Normals"))
		ioMesh.CalculateNormals();

	ImGui::SameLine();

	if (ImGui::Button("Generate Tangents"))
		ioMesh.CalculateTangents();

	return scene_changed;
}


bool InspectorWidget::DrawComponent(Entity inEntity, Camera& ioCamera)
{
	return false;
}

bool InspectorWidget::DrawComponent(Entity inEntity, SoftBody& ioSoftBody)
{
	JPH::BodyInterface& body_interface = GetPhysics().GetSystem()->GetBodyInterface();

	ImGui::Text("Unique Body ID: %i", ioSoftBody.mBodyID.GetIndexAndSequenceNumber());

	Mesh& mesh = GetScene().Get<Mesh>(inEntity);
	Transform& transform = GetScene().Get<Transform>(inEntity);

	if (ImGui::Button("Build"))
	{
		// verts
		for (const Vec3& pos : mesh.positions)
		{
			const Vec3 wpos = pos * transform.scale;

			JPH::SoftBodySharedSettings::Vertex v;
			v.mPosition = JPH::Float3(wpos.x, wpos.y, wpos.z);

			ioSoftBody.mSharedSettings.mVertices.push_back(v);
		}

		// faces
		for (int index = 0; index < mesh.indices.size(); index += 3)
		{
			JPH::SoftBodySharedSettings::Face face;
			face.mVertex[0] = mesh.indices[index];
			face.mVertex[1] = mesh.indices[index + 1];
			face.mVertex[2] = mesh.indices[index + 2];

			ioSoftBody.mSharedSettings.AddFace(face);
		}

		const int ntriangles = mesh.indices.size() / 3;
		int		maxidx = 0;
		int i, j, ni;

		for (uint32_t index : mesh.indices)
			maxidx = glm::max((int)index, maxidx);
		++maxidx;

		Array<bool> chks;
		chks.resize(maxidx * maxidx, false);

		for (int i = 0, ni = ntriangles * 3; i < ni; i += 3)
		{
			const int idx[] = { (int)mesh.indices[i], (int)mesh.indices[i + 1], (int)mesh.indices[i + 2] };
#define IDX(_x_,_y_) ((_y_)*maxidx+(_x_))
			for (int j = 2, k = 0; k < 3; j = k++)
			{
				if (!chks[IDX(idx[j], idx[k])])
				{
					chks[IDX(idx[j], idx[k])] = true;
					chks[IDX(idx[k], idx[j])] = true;
					ioSoftBody.mSharedSettings.mEdgeConstraints.push_back(JPH::SoftBodySharedSettings::Edge(idx[j], idx[k], 0.00001f));
				}
			}
#undef IDX
		}

		//// edges
		//for (auto index = 0u; index < 24; index += 3) {
		//    JPH::SoftBodySharedSettings::Edge edge;
		//    edge.mCompliance = 0.00001f;
		//    
		//    edge.mVertex[0] = mesh.indices[index];
		//    edge.mVertex[1] = mesh.indices[index + 1];
		//    soft_body.mSharedSettings.mEdgeConstraints.push_back(edge);

		//    edge.mVertex[0] = mesh.indices[index];
		//    edge.mVertex[1] = mesh.indices[index + 2];
		//    soft_body.mSharedSettings.mEdgeConstraints.push_back(edge);

		//    edge.mVertex[0] = mesh.indices[index + 1];
		//    edge.mVertex[1] = mesh.indices[index + 2];
		//    soft_body.mSharedSettings.mEdgeConstraints.push_back(edge);
		//}



		ioSoftBody.mSharedSettings.CalculateEdgeLengths();

		ioSoftBody.mCreationSettings.mUpdatePosition = false;
		ioSoftBody.mCreationSettings.mGravityFactor = 0.0f;
		ioSoftBody.mCreationSettings.mLinearDamping = 0.0f;
		ioSoftBody.mCreationSettings.mMaxLinearVelocity = 5.0f;

		ioSoftBody.mSharedSettings.SetEmbedded();
	}

	if (ImGui::Button("Add Impulse"))
	{
		GetPhysics().GetSystem()->GetBodyInterface().AddImpulse(ioSoftBody.mBodyID, JPH::Vec3(0.0f, 0.0f, 1.0f));
	}

	return false;
}


bool InspectorWidget::DrawComponent(Entity inEntity, RigidBody& inBoxCollider)
{
	JPH::BodyInterface& body_interface = GetPhysics().GetSystem()->GetBodyInterface();

	ImGui::Text("Unique Body ID: %i", inBoxCollider.bodyID.GetIndexAndSequenceNumber());

	constexpr std::array items = { "Static", "Kinematic", "Dynamic" };

	int index = int(inBoxCollider.motionType);
	if (ImGui::Combo("##MotionType", &index, items.data(), int(items.size())))
	{
		inBoxCollider.motionType = JPH::EMotionType(index);
		body_interface.SetMotionType(inBoxCollider.bodyID, inBoxCollider.motionType, JPH::EActivation::Activate);
	}

	if (inBoxCollider.motionType == JPH::EMotionType::Static)
	{
		if (ImGui::Button("Create Mesh Shape"))
		{
			Mesh* mesh = GetScene().GetPtr<Mesh>(inEntity);
			Transform* transform = GetScene().GetPtr<Transform>(inEntity);

			if (mesh && transform)
				inBoxCollider.CreateMeshCollider(*mesh, *transform);
		}
	}

	if (ImGui::Button("Create Cube Shape"))
	{
		Mesh* mesh = GetScene().GetPtr<Mesh>(inEntity);
		Transform* transform = GetScene().GetPtr<Transform>(inEntity);

		if (mesh && transform)
			inBoxCollider.CreateCubeCollider(mesh->bbox.Scaled(transform->scale));
		else
			inBoxCollider.CreateCubeCollider(BBox3D(Vec3(0.0f), Vec3(1.0f)));
	}

	if (ImGui::Button("Create Cylinder Shape"))
	{
		Mesh* mesh = GetScene().GetPtr<Mesh>(inEntity);
		Transform* transform = GetScene().GetPtr<Transform>(inEntity);

		if (mesh && transform)
			inBoxCollider.CreateCylinderCollider(*mesh, *transform);
	}

	return false;
}


bool InspectorWidget::DrawComponent(Entity inEntity, Skeleton& inSkeleton)
{
	uint32_t bone_count = 0;

	auto CountBone = [&](auto&& CountBone, Skeleton::Bone& inBone) -> void
	{ 
		bone_count++; 

		for (Skeleton::Bone& child_bone : inBone.children)
			CountBone(CountBone, child_bone);
	};

	CountBone(CountBone, inSkeleton.rootBone);

	static bool show_bones = false;
	ImGui::Checkbox("##ShowBones", &show_bones);

	ImGui::SameLine();

	ImGui::Text("Show %i Bones", bone_count);

	if (show_bones) 
	{
		Mat4x4 world_transform(1.0f);

		if (Transform* transform = m_Editor->GetScene()->GetPtr<Transform>(inEntity))
			world_transform = transform->worldTransform;

		inSkeleton.DebugDraw(inSkeleton.rootBone, world_transform);
	}

	Scene& scene = GetScene();
	if (scene.Has<Animation>(inSkeleton.animation) && scene.Has<Animation, Name>(inSkeleton.animation))
	{
		const auto& [animation, name] = scene.Get<Animation, Name>(inSkeleton.animation);

		if (ImGui::DragDropTargetButton("Animation", name.name.c_str(), true))
			SetActiveEntity(inSkeleton.animation);
	}
	else
		ImGui::DragDropTargetButton("Animation", "None", false);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
		{
			Entity entity = *reinterpret_cast<const Entity*>( payload->Data );

			if (scene.Has<Animation>(entity))
				inSkeleton.animation = entity;
		}

		ImGui::EndDragDropTarget();
	}

	if (ImGui::Button("Save as graph.."))
	{
		const String file_path = OS::sSaveFileDialog("DOT File (*.dot)\0", "dot");

		if (!file_path.empty())
		{
			std::ofstream ofs(file_path);

			ofs << "digraph G {\n";

			auto traverse = [&](auto&& traverse, const Skeleton::Bone& boneNode) -> void
			{
				for (const Skeleton::Bone& child_bone : boneNode.children)
				{
					ofs << "\"" << boneNode.name << "\" -> \"" << child_bone.name << "\";\n";
					traverse(traverse, child_bone);
				}
			};

			traverse(traverse, inSkeleton.rootBone);
			ofs << "}";
		}
	}

	return false;
}


bool InspectorWidget::DrawComponent(Entity inEntity, Material& inMaterial)
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	const float line_height = io.FontDefault->FontSize;

	bool scene_changed = false;

	scene_changed |= ImGui::ColorEdit4("Albedo", glm::value_ptr(inMaterial.albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	scene_changed |= ImGui::ColorEdit3("Emissive", glm::value_ptr(inMaterial.emissive), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

	scene_changed |= ImGui::DragFloat("Metallic", &inMaterial.metallic, 0.001f, 0.0f, 1.0f);
	scene_changed |= ImGui::DragFloat("Roughness", &inMaterial.roughness, 0.001f, 0.0f, 1.0f);

	//ImGui::Text(inMaterial.vertexShaderFile.c_str());
	//ImGui::SameLine();

	//if (ImGui::Button((const char*)ICON_FA_FOLDER))
	//{
	//	String filepath = fs::relative(OS::sOpenFileDialog("HLSL Files (*.hlsl)\0*.hlsl\0")).string();

	//	if (!filepath.empty())
	//	{
	//		inMaterial.vertexShaderFile = filepath;
	//		m_Editor->GetRenderInterface()->CompileMaterialShaders(inEntity, inMaterial);
	//	}
	//}

	ImGui::AlignTextToFramePadding(); ImGui::Text("Vertex Shader   "); ImGui::SameLine();

	ImGui::PushID("selectfilevertexshader");
	if (ImGui::Button((const char*)ICON_FA_FOLDER))
	{
		String filepath = fs::relative(OS::sOpenFileDialog("HLSL Files (*.hlsl)\0*.hlsl\0")).string();

		if (!filepath.empty())
		{
			scene_changed = true;
			inMaterial.vertexShaderFile = filepath;
			m_Editor->GetRenderInterface()->CompileMaterialShaders(inEntity, inMaterial);
		}
	}

	ImGui::PopID();
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();

	{
		const char* file_text = inMaterial.vertexShaderFile.empty() ? "N/A" : inMaterial.vertexShaderFile.c_str();
		const char* tooltip_text = inMaterial.vertexShaderFile.empty() ? "No file loaded." : inMaterial.vertexShaderFile.c_str();

		ImGui::Text(file_text);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(tooltip_text);
	}

	ImGui::AlignTextToFramePadding(); ImGui::Text("Pixel Shader      "); ImGui::SameLine();

	ImGui::PushID("selectfilepixelshader");
	if (ImGui::Button((const char*)ICON_FA_FOLDER))
	{
		String filepath = fs::relative(OS::sOpenFileDialog("HLSL Files (*.hlsl)\0*.hlsl\0")).string();

		if (!filepath.empty())
		{
			scene_changed = true;
			inMaterial.pixelShaderFile = filepath;
			m_Editor->GetRenderInterface()->CompileMaterialShaders(inEntity, inMaterial);
		}
	}
	
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();

	{
		const char* file_text = inMaterial.pixelShaderFile.empty() ? "N/A" : inMaterial.pixelShaderFile.c_str();
		const char* tooltip_text = inMaterial.pixelShaderFile.empty() ? "No file loaded." : inMaterial.pixelShaderFile.c_str();

		ImGui::Text(file_text);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(tooltip_text);
	}

	if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{

		ImGui::Text("Asset conversion failed. See console output log.\n");

		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	auto DrawSwizzleChannel = [&](const char* inLabel, uint8_t& inSwizzle, uint8_t inSwizzleMode)
	{
		const bool is_selected = ( inSwizzle == inSwizzleMode ) || inSwizzle == TEXTURE_SWIZZLE_RGBA;

		if (is_selected)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

		if (ImGui::SmallButton(inLabel))
		{
			if (inSwizzle == inSwizzleMode)
				inSwizzle = TEXTURE_SWIZZLE_RGBA;
			else
				inSwizzle = inSwizzleMode;
		}

		if (is_selected)
			ImGui::PopStyleColor();
	};

	auto DrawTextureInteraction = [&](uint32_t& inGpuMap, uint8_t& inSwizzle, uint32_t inDefaultMap, std::string& inFile, uint32_t& imguiID)
	{
		/*ImGui::PushID(imguiID++);
		DrawSwizzleChannel("R", inSwizzle, TEXTURE_SWIZZLE_RRRR);
		ImGui::PopID();
		ImGui::SameLine();

		ImGui::PushID(imguiID++);
		DrawSwizzleChannel("G", inSwizzle, TEXTURE_SWIZZLE_GGGG);
		ImGui::PopID();
		ImGui::SameLine();

		ImGui::PushID(imguiID++);
		DrawSwizzleChannel("B", inSwizzle, TEXTURE_SWIZZLE_BBBB);
		ImGui::PopID();
		ImGui::SameLine();

		ImGui::PushID(imguiID++);
		DrawSwizzleChannel("A", inSwizzle, TEXTURE_SWIZZLE_AAAA);
		ImGui::PopID();*/

		const uint32_t imgui_map = inGpuMap ? inGpuMap : inDefaultMap;
		const uint64_t imgui_id = m_Editor->GetRenderInterface()->GetImGuiTextureID(imgui_map);

		if (ImGui::ImageButton((void*)( (intptr_t)imgui_id ), ImVec2(line_height - 1, line_height - 1)))
		{
			String filepath = OS::sOpenFileDialog("Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0");

			if (!filepath.empty())
			{
				const String asset_path = TextureAsset::Convert(filepath);

				if (!asset_path.empty())
				{
					inFile = asset_path;
					const bool is_srgb = inGpuMap == inMaterial.gpuAlbedoMap;
					inGpuMap = m_Editor->GetRenderInterface()->UploadTextureFromAsset(GetAssets().GetAsset<TextureAsset>(asset_path), is_srgb);
				}
				else
					ImGui::OpenPopup("Error");
			}
		}

		if (!inFile.empty() && inGpuMap && inGpuMap != inDefaultMap && ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Open"))
			{
				ShellExecute(NULL, "open", inFile.c_str(), NULL, NULL, SW_RESTORE);
			}

			if (ImGui::MenuItem("Remove"))
			{
				inFile = "";
				inGpuMap = inDefaultMap;
				scene_changed = true;
			}

			if (ImGui::MenuItem("Show In Explorer"))
			{
				String shell_command = std::format("/select,{}", inFile);
				ShellExecute(0, NULL, "explorer.exe", shell_command.c_str(), NULL, SW_SHOWNORMAL);
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		
		const char* file_text = inFile.empty() ? "N/A" : inFile.c_str();
		const char* tooltip_text = inFile.empty() ? "No file loaded." : inFile.c_str();

		ImGui::Text(file_text);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(tooltip_text);
	};

	uint32_t imgui_id = 0;

	ImGui::AlignTextToFramePadding(); ImGui::Text("Albedo Map       "); ImGui::SameLine();
	DrawTextureInteraction(inMaterial.gpuAlbedoMap, inMaterial.gpuAlbedoMapSwizzle, Material::Default.gpuAlbedoMap, inMaterial.albedoFile, imgui_id);
	ImGui::AlignTextToFramePadding(); ImGui::Text("Normal Map       "); ImGui::SameLine();
	DrawTextureInteraction(inMaterial.gpuNormalMap, inMaterial.gpuNormalMapSwizzle, Material::Default.gpuNormalMap, inMaterial.normalFile, imgui_id);
	ImGui::AlignTextToFramePadding(); ImGui::Text("Emissive Map    "); ImGui::SameLine();
	DrawTextureInteraction(inMaterial.gpuEmissiveMap, inMaterial.gpuEmissiveMapSwizzle, Material::Default.gpuEmissiveMap, inMaterial.emissiveFile, imgui_id);
	ImGui::AlignTextToFramePadding(); ImGui::Text("Metallic Map      "); ImGui::SameLine();
	DrawTextureInteraction(inMaterial.gpuMetallicMap, inMaterial.gpuMetallicMapSwizzle, Material::Default.gpuMetallicMap, inMaterial.metallicFile, imgui_id);
	ImGui::AlignTextToFramePadding(); ImGui::Text("Roughness Map"); ImGui::SameLine();
	DrawTextureInteraction(inMaterial.gpuRoughnessMap, inMaterial.gpuRoughnessMapSwizzle, Material::Default.gpuRoughnessMap, inMaterial.roughnessFile, imgui_id);

	/* for (const auto& [index, texture] : gEnumerate(inMaterial.textures))
	{
		DrawTextureInteraction(texture.gpuMap, texture.swizzle, Material::Default.textures[index].gpuMap, texture.filepath, imgui_id);
	} */

	scene_changed |= ImGui::Checkbox("Is Transparent", &inMaterial.isTransparent);

	return scene_changed;
}


bool InspectorWidget::DrawComponent(Entity inEntity, Transform& inTransform)
{
	bool scene_changed = false;

    ImGui::SetNextItemRightAlign("Scale     ");
	if (ImGui::DragVec3("##ScaleDragFloat3", inTransform.scale, 0.001f, 0.0f, FLT_MAX))
	{
		inTransform.Compose();
		scene_changed = true;
	}

    ImGui::SetNextItemRightAlign("Rotation");
	Vec3 degrees = glm::degrees(glm::eulerAngles(inTransform.rotation));
	if (ImGui::DragVec3("##RotationDragFloat3", degrees, 0.1f, -360.0f, 360.0f))
	{
		inTransform.rotation = glm::quat(glm::radians(degrees));
		inTransform.Compose();
		scene_changed = true;
	}

    ImGui::SetNextItemRightAlign("Position ");
	if (ImGui::DragVec3("##PositionDragFloat3", inTransform.position, 0.001f, -FLT_MAX, FLT_MAX))
	{
		inTransform.Compose();
		scene_changed = true;
	}

	ImGui::Separator();

	const Animation* animation = GetScene().GetPtr<Animation>(inTransform.animation);
	const String preview_text = animation ? animation->GetName() : "None";

	if (ImGui::DragDropTargetButton("Animation", preview_text.c_str(), animation != nullptr))
	{
		SetActiveEntity(inTransform.animation);
		scene_changed = true;
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
		{
			inTransform.animation = *reinterpret_cast<const Entity*>( payload->Data );
			scene_changed = true;
		}

		ImGui::EndDragDropTarget();
	}

	if (animation != nullptr)
	{
		if (ImGui::BeginCombo("Channel", inTransform.animationChannel.c_str()))
		{
			static HashSet<String> channels;
			channels.clear();

			for (const String& bone_name : animation->GetBoneNames())
				channels.insert(bone_name);

			for (const String& channel : channels)
			{
				if (ImGui::Selectable(channel.c_str(), inTransform.animationChannel == channel))
				{
					inTransform.animationChannel = channel;
					scene_changed = true;
				}
			}

			ImGui::EndCombo();
		}
	}

	return scene_changed;
}


bool InspectorWidget::DrawComponent(Entity inEntity, Animation& inAnimation)
{
	const float current_time = inAnimation.GetRunningTime();
	const float total_duration = inAnimation.GetTotalDuration();

	ImGui::Text("Bone Count: %i", inAnimation.GetBoneCount());
	ImGui::Text("Total Duration: %.2f seconds", total_duration / 1000.0f);

	String name = inAnimation.GetName();
	if (ImGui::InputText("Name##Animation::m_Name", &name, ImGuiInputTextFlags_EnterReturnsTrue))
		inAnimation.SetName(name);

	bool scene_changed = false;

	if (ImGui::Button("Apply to Scene"))
	{
		for (const auto& [entity, name, transform] : GetScene().Each<Name, Transform>())
		{
			if (inAnimation.HasKeyFrames(name))
			{
				transform.animation = inEntity;
				transform.animationChannel = name;
			}
		}

		scene_changed = true;
	}

	if (ImGui::Button("Remove from Scene"))
	{
		for (const auto& [entity, transform] : GetScene().Each<Transform>())
		{
			if (transform.animation == inEntity)
			{
				transform.animation = Entity::Null;
				transform.animationChannel = "";
			}
		}

		scene_changed = true;
	}

	if (inAnimation.IsPlaying())
	{
		if (ImGui::Button(reinterpret_cast<const char*>( ICON_FA_STOP )))
		{
			scene_changed = true;
			inAnimation.SetIsPlaying(false);
		}

		ImGui::SameLine();

		char overlay_buf[32];
		ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.2f seconds", current_time / 1000.0f);

		ImGui::ProgressBar(current_time / total_duration, ImVec2(-FLT_MIN, 0), overlay_buf);

		ImGui::SameLine();
		ImGui::Text("Time");
	}
	else
	{
		if (ImGui::Button(reinterpret_cast<const char*>( ICON_FA_PLAY )))
		{
			scene_changed = true;
			inAnimation.SetIsPlaying(true);
		}

		ImGui::SameLine();

		float time = inAnimation.GetRunningTime() / 1000.0f;
		if (ImGui::SliderFloat("Time", &time, 0.0f, inAnimation.GetTotalDuration() / 1000.0f))
		{
			scene_changed = true;
			inAnimation.SetRunningTime(time * 1000.0f);
		}
	}

	return scene_changed;
}


bool InspectorWidget::DrawComponent(Entity inEntity, Light& inLight)
{
	bool scene_changed = false;

	constexpr std::array type_names = { "None", "Spot", "Point" };
	scene_changed |= ImGui::Combo("Type", (int*)&inLight.type, type_names.data(), type_names.size());

	scene_changed |= ImGui::ColorEdit3("Colour", glm::value_ptr(inLight.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	scene_changed |= ImGui::DragFloat("Intensity", &inLight.colour.a, 0.001f);

	switch (inLight.type)
	{
		case LIGHT_TYPE_POINT:
		{
			scene_changed |= ImGui::DragFloat("Radius", &inLight.attributes.x, 0.001f, 0.0f, 100.0f);
		} break;

		case LIGHT_TYPE_SPOT:
		{
			scene_changed |= ImGui::DragFloat("Range", &inLight.attributes.x, 0.001f);
			scene_changed |= ImGui::DragFloat("Inner Cone Angle", &inLight.attributes.y, 0.001f, 0.0f, M_PI / 2);
			scene_changed |= ImGui::DragFloat("Outer Cone Angle", &inLight.attributes.z, 0.001f, 0.0f, M_PI / 2);
			scene_changed |= ImGui::DragFloat3("Direction", &inLight.direction[0], 0.001f);
		} break;
	}

	return scene_changed;
}


bool InspectorWidget::DrawComponent(Entity inEntity, NativeScript& inScript)
{
	Scene& scene = GetScene();
	Assets& assets = GetAssets();

	if (assets.ContainsAsset(inScript.file))
	{
		ImGui::Text("File: %s", inScript.file.c_str());
	}

	if (ImGui::Button("Load.."))
	{
		const String filepath = OS::sOpenFileDialog("DLL Files (*.dll)\0*.dll\0");

		if (!filepath.empty())
		{
			inScript.file = fs::relative(filepath).string();

			if (ScriptAsset::Ptr asset = assets.GetAsset<ScriptAsset>(inScript.file))
			{
				for (const String& type_str : asset->GetRegisteredTypes())
					inScript.types.push_back(type_str);
			}
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Reload"))
	{
		assets.ReleaseAsset(inScript.file);
		assets.GetAsset<ScriptAsset>(inScript.file);
		
		scene.BindScriptToEntity(GetActiveEntity(), inScript, m_Editor);
	}

	ImGui::SameLine();

	if (ImGui::Button("Release"))
	{
		assets.ReleaseAsset(inScript.file);
	}

    if (!inScript.types.empty())
    {
		auto type_iter = std::find(inScript.types.begin(), inScript.types.end(), inScript.type);

        const char* preview_text = type_iter == inScript.types.end() ? "None" : inScript.type.c_str();

        if (ImGui::BeginCombo("Type", preview_text, ImGuiComboFlags_None))
        {
			for (auto iter = inScript.types.begin(); iter != inScript.types.end(); iter++)
            {
                if (ImGui::Selectable(iter->c_str(), iter == type_iter))
                {
					inScript.type = *iter;
                    scene.BindScriptToEntity(GetActiveEntity(), inScript, m_Editor);
                }
            }

            ImGui::EndCombo();
        }
    }
    else
    {
        if (ImGui::BeginCombo("Type", "None Found", ImGuiComboFlags_None))
            ImGui::EndCombo();
    }

	ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.5f));
	ImGui::SeparatorText("Variables");
	ImGui::PopStyleVar();

	if (const RTTI* rtti = g_RTTIFactory.GetRTTI(inScript.type.c_str()))
	{
		for (const auto& member : *rtti)
		{
			if (const RTTI* member_rtti = member->GetRTTI())
			{
				gForEachType<int, float, bool, Entity>([&](auto type)
				{
					if (*member_rtti == RTTI_OF(decltype( type )))
					{
						DrawScriptMember(member->GetCustomName(), member->GetRef<decltype( type )>(inScript.script));
						return;
					}
				});
			}
			else
			{
				ImGui::Text(member->GetCustomName());
			}
		}
	}

	return false;
}


bool InspectorWidget::DrawComponent(Entity inEntity, DDGISceneSettings& ioSettings)
{
	const Transform& transform = GetScene().Get<Transform>(GetActiveEntity());

	const Vec3 min_bounds = transform.GetPositionWorldSpace();
	const Vec3 max_bounds = min_bounds + ioSettings.mDDGIProbeSpacing * Vec3(ioSettings.mDDGIProbeCount);

	g_DebugRenderer.AddLineCube(min_bounds, max_bounds);

	if (ImGui::Button("Fit to Scene", ImVec2(ImGui::GetWindowWidth(), 0)))
		ioSettings.FitToScene(GetScene(), GetScene().Get<Transform>(GetActiveEntity()));

	ImGui::DragInt3("Probe Count", glm::value_ptr(ioSettings.mDDGIProbeCount), 1, 1, 40);
	ImGui::DragFloat3("Probe Spacing", glm::value_ptr(ioSettings.mDDGIProbeSpacing), 0.01f, -1000.0f, 1000.0f, "%.3f");

	return false;
}


bool InspectorWidget::DrawComponent(Entity inEntity, DirectionalLight& inDirectionalLight)
{
	bool scene_changed = false;

	ImGui::SetNextItemRightAlign("Colour     ");
	scene_changed |= ImGui::ColorEdit3("##Colour", glm::value_ptr(inDirectionalLight.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

	ImGui::SetNextItemRightAlign("Intensity  ");
	scene_changed |= ImGui::DragFloat("##Intensity", &inDirectionalLight.colour.a, 0.001f, 0.0f, FLT_MAX);

	return scene_changed;
}

}