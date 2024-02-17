#include "pch.h"
#include "inspectorWidget.h"

#include "OS.h"
#include "gui.h"
#include "json.h"
#include "iter.h"
#include "scene.h"
#include "debug.h"
#include "script.h"
#include "archive.h"
#include "systems.h"
#include "physics.h"
#include "components.h"
#include "application.h"

#include "viewportWidget.h"
#include "SequenceWidget.h"
#include "NodeGraphWidget.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(InspectorWidget) {}


InspectorWidget::InspectorWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_INFO_CIRCLE " Inspector " )) {}


void InspectorWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();

	auto viewport_widget = inWidgets->GetWidget<ViewportWidget>();
    auto sequence_widget = inWidgets->GetWidget<SequenceWidget>();
	auto nodegraph_widget = inWidgets->GetWidget<NodeGraphWidget>();

	if (viewport_widget && viewport_widget->IsVisible() && GetActiveEntity() != NULL_ENTITY)
	{
		DrawEntityInspector(inWidgets);
	}
    else if (sequence_widget && sequence_widget->IsVisible() && sequence_widget->GetSelectedKeyFrame() != -1)
    {
        DrawKeyFrameInspector(inWidgets);
    }
	else if (nodegraph_widget && nodegraph_widget->IsVisible())
	{
		DrawJSONInspector(inWidgets);
	}

	ImGui::End();
};


void InspectorWidget::DrawEntityInspector(Widgets* inWidgets)
{
	auto active_entity = m_Editor->GetActiveEntity();
	if (active_entity == NULL_ENTITY)
		return;

	ImGui::Text("Entity ID: %i", active_entity);

	auto& scene = GetScene();
	auto& assets = GetAssets();

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
				if (ImGui::CollapsingHeader(components.name, &is_open, ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PopStyleVar();

					if (is_open)
						DrawComponent(scene.Get<ComponentType>(entity));
					else
						scene.Remove<ComponentType>(entity);
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
		if (ImGui::Selectable("Native Script", false))
		{
			scene.Add<NativeScript>(active_entity);
			ImGui::CloseCurrentPopup();
		}

		if (scene.Has<Transform, Mesh>(active_entity))
		{
			const auto& [transform, mesh] = scene.Get<Transform, Mesh>(active_entity);

			if (ImGui::Selectable("Box Collider", false))
			{
				auto& collider = scene.Add<BoxCollider>(active_entity);

				const auto half_extent = glm::abs(mesh.aabb[1] - mesh.aabb[0]) / 2.0f * transform.scale;
				collider.settings.mHalfExtent = JPH::Vec3(half_extent.x, half_extent.y, half_extent.z);
				//collider.settings.SetEmbedded();
			}

			if (ImGui::Selectable("Soft Body", false))
			{
				auto& soft_body = scene.Add<SoftBody>(active_entity);
				auto& transform = scene.Get<Transform>(active_entity);

				// verts
				for (const auto& pos : mesh.positions)
				{
					const auto wpos = pos * transform.scale;

					JPH::SoftBodySharedSettings::Vertex v;
					v.mPosition = JPH::Float3(wpos.x, wpos.y, wpos.z);

					soft_body.mSharedSettings.mVertices.push_back(v);
				}

				// faces
				for (auto index = 0u; index < mesh.indices.size(); index += 3)
				{
					JPH::SoftBodySharedSettings::Face face;
					face.mVertex[0] = mesh.indices[index];
					face.mVertex[1] = mesh.indices[index + 1];
					face.mVertex[2] = mesh.indices[index + 2];

					soft_body.mSharedSettings.AddFace(face);
				}

				const auto ntriangles = mesh.indices.size() / 3;
				int		maxidx = 0;
				int i, j, ni;

				for (auto index : mesh.indices)
					maxidx = glm::max((int)index, maxidx);
				++maxidx;

				std::vector<bool> chks;
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
							soft_body.mSharedSettings.mEdgeConstraints.push_back(JPH::SoftBodySharedSettings::Edge(idx[j], idx[k], 0.00001f));
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



				soft_body.mSharedSettings.CalculateEdgeLengths();

				soft_body.mCreationSettings.mUpdatePosition = false;
				soft_body.mCreationSettings.mGravityFactor = 0.0f;
				soft_body.mCreationSettings.mLinearDamping = 0.0f;
				soft_body.mCreationSettings.mMaxLinearVelocity = 5.0f;

				soft_body.mSharedSettings.SetEmbedded();
			}
		}

		ImGui::EndPopup();
	}

	// Broken for now
	if (ImGui::Button("Add Component", ImVec2(ImGui::GetWindowWidth(), 0)))
		ImGui::OpenPopup("Components");

	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
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


void InspectorWidget::DrawScriptMember(const char* inLabel, uint32_t& ioValue)
{
	int temp_value = ioValue;
	if (ImGui::InputInt(inLabel, &temp_value))
		ioValue = temp_value;

	if (ImGui::BeginDragDropTarget())
	{
		if (const auto payload = ImGui::AcceptDragDropPayload("drag_drop_hierarchy_entity"))
			ioValue = *reinterpret_cast<const uint32_t*>( payload->Data );
	}
}



void InspectorWidget::DrawComponent(Name& inName)
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
}


void InspectorWidget::DrawComponent(Node& ioNode)
{
	if (ioNode.parent != NULL_ENTITY)
	{
		ImGui::Text("Parent entity:");
		ImGui::SameLine();

		auto parent = std::to_string(ioNode.parent);

		if (ImGui::SmallButton(parent.c_str()))
			SetActiveEntity(ioNode.parent);

		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
			NodeSystem::sRemove(GetScene(), ioNode);
	}
	else
		ImGui::Text("Parent entity: NULL");

	ImGui::Text("First Child: %i", ioNode.firstChild);
	ImGui::Text("Siblings: %i, %i", ioNode.prevSibling, ioNode.nextSibling);
}


void InspectorWidget::DrawComponent(Mesh& ioMesh)
{
	const auto byte_size = sizeof(Mesh) + ioMesh.GetInterleavedStride() * ioMesh.positions.size();
	ImGui::Text("%.1f Kb", float(byte_size) / 1024);

	ImGui::Text("%i Meshlets", ioMesh.meshlets.size());
	ImGui::Text("%i Vertices", ioMesh.positions.size());
	ImGui::Text("%i Triangles", ioMesh.indices.size() / 3);


	auto& scene = GetScene();
	if (scene.Has<Material>(ioMesh.material) && scene.Has<Material, Name>(ioMesh.material))
	{
		const auto& [material, name] = scene.Get<Material, Name>(ioMesh.material);

		const auto albedo_imgui_id = m_Editor->GetRenderInterface()->GetImGuiTextureID(material.gpuAlbedoMap);
		const auto preview_size = ImVec2(10 * ImGui::GetWindowDpiScale(), 10 * ImGui::GetWindowDpiScale());
		const auto tint_color = ImVec4(material.albedo.r, material.albedo.g, material.albedo.b, material.albedo.a);

		if (ImGui::ImageButton((void*)(intptr_t)albedo_imgui_id, preview_size))
			SetActiveEntity(ioMesh.material);

		ImGui::SameLine();
		ImGui::Text(name.name.c_str());
	}
	else
		ImGui::Text("No material");

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material"))
			ioMesh.material = *reinterpret_cast<const Entity*>( payload->Data );

		ImGui::EndDragDropTarget();
	}

	ImGui::DragFloat("LOD Fade", &ioMesh.mLODFade, 0.001f, -1.0f, 1.0f, "%.3f");

	if (ImGui::Button("Generate Normals"))
		ioMesh.CalculateNormals();

	ImGui::SameLine();

	if (ImGui::Button("Generate Tangents"))
		ioMesh.CalculateTangents();
}


void InspectorWidget::DrawComponent(SoftBody& ioSoftBody)
{
	auto& body_interface = GetPhysics().GetSystem()->GetBodyInterface();

	ImGui::Text("Unique Body ID: %i", ioSoftBody.mBodyID.GetIndexAndSequenceNumber());

	if (ImGui::Button("Add Impulse"))
	{
		GetPhysics().GetSystem()->GetBodyInterface().AddImpulse(ioSoftBody.mBodyID, JPH::Vec3(0.0f, 0.0f, 1.0f));
	}
}


void InspectorWidget::DrawComponent(BoxCollider& inBoxCollider)
{
	auto& body_interface = GetPhysics().GetSystem()->GetBodyInterface();

	ImGui::Text("Unique Body ID: %i", inBoxCollider.bodyID.GetIndexAndSequenceNumber());

	constexpr auto items = std::array { "Static", "Kinematic", "Dynamic" };

	auto index = int(inBoxCollider.motionType);
	if (ImGui::Combo("##MotionType", &index, items.data(), int(items.size())))
	{
		inBoxCollider.motionType = JPH::EMotionType(index);
		body_interface.SetMotionType(inBoxCollider.bodyID, inBoxCollider.motionType, JPH::EActivation::Activate);
	}
}


void InspectorWidget::DrawComponent(Skeleton& inSkeleton)
{
	static bool show_bones = false;
	ImGui::Checkbox("Show Bones", &show_bones);
	
	ImGui::Text("%i Bones", inSkeleton.boneHierarchy.children.size());

	if (show_bones)
		inSkeleton.DebugDraw(inSkeleton.boneHierarchy);

	if (ImGui::BeginCombo("Animation##InspectorWidget::drawComponent", inSkeleton.animations[0].GetName().c_str()))
	{
		for (auto& animation : inSkeleton.animations)
			ImGui::Selectable(animation.GetName().c_str());

		ImGui::EndCombo();
	}

	if (ImGui::Button(inSkeleton.autoPlay ? (const char*)ICON_FA_STOP : (const char*)ICON_FA_PLAY))
		inSkeleton.autoPlay = !inSkeleton.autoPlay;

	ImGui::SameLine();

	const  auto current_time = inSkeleton.animations[0].GetRunningTime();
	const  auto total_duration = inSkeleton.animations[0].GetTotalDuration();

	if (inSkeleton.autoPlay)
	{
		char overlay_buf[32];
		ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.2f seconds", current_time / 1000.0f);

		ImGui::ProgressBar(current_time / total_duration, ImVec2(-FLT_MIN, 0), overlay_buf);

		ImGui::SameLine();
		ImGui::Text("Time");
	}
	else
	{
		float time = inSkeleton.currentTime / 1000.0f;
		if (ImGui::SliderFloat("Time", &time, 0.0f, inSkeleton.animations[0].GetTotalDuration() / 1000.0f))
			inSkeleton.currentTime = time * 1000.0f;
	}

	if (ImGui::Button("Save as graph.."))
	{
		const auto file_path = OS::sSaveFileDialog("DOT File (*.dot)\0", "dot");

		if (!file_path.empty())
		{
			auto ofs = std::ofstream(file_path);

			ofs << "digraph G {\n";

			auto traverse = [&](auto&& traverse, const Bone& boneNode) -> void
			{
				for (const auto& child : boneNode.children)
				{
					ofs << "\"" << boneNode.name << "\" -> \"" << child.name << "\";\n";
					traverse(traverse, child);
				}
			};

			traverse(traverse, inSkeleton.boneHierarchy);
			ofs << "}";
		}
	}

}


void InspectorWidget::DrawComponent(Material& inMaterial)
{
	auto& io = ImGui::GetIO();
	auto& style = ImGui::GetStyle();
	const auto line_height = io.FontDefault->FontSize;

	ImGui::ColorEdit4("Albedo", glm::value_ptr(inMaterial.albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	ImGui::ColorEdit3("Emissive", glm::value_ptr(inMaterial.emissive), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

	ImGui::DragFloat("Metallic", &inMaterial.metallic, 0.001f, 0.0f, 1.0f);
	ImGui::DragFloat("Roughness", &inMaterial.roughness, 0.001f, 0.0f, 1.0f);

	if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{

		ImGui::Text("Asset conversion failed. See console output log.\n");

		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	auto DrawSwizzleChannel = [&](const char* inLabel, uint8_t& inSwizzle, uint8_t inSwizzleMode)
	{
		const auto is_selected = ( inSwizzle == inSwizzleMode ) || inSwizzle == TEXTURE_SWIZZLE_RGBA;

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

		const auto imgui_map = inGpuMap ? inGpuMap : inDefaultMap;
		const auto imgui_id = m_Editor->GetRenderInterface()->GetImGuiTextureID(imgui_map);

		if (ImGui::ImageButton((void*)( (intptr_t)imgui_id ), ImVec2(line_height - 1, line_height - 1)))
		{
			auto filepath = OS::sOpenFileDialog("Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0");

			if (!filepath.empty())
			{
				const auto asset_path = TextureAsset::sConvert(filepath);

				if (!asset_path.empty())
				{
					inFile = asset_path;
					const auto is_srgb = inGpuMap == inMaterial.gpuAlbedoMap;
					inGpuMap = m_Editor->GetRenderInterface()->UploadTextureFromAsset(GetAssets().GetAsset<TextureAsset>(asset_path), is_srgb);
				}
				else
					ImGui::OpenPopup("Error");
			}
		}

		if (!inFile.empty() && inGpuMap && inGpuMap != inDefaultMap && ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Remove"))
			{
				inFile = "";
				inGpuMap = inDefaultMap;
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		
		const auto file_text = inFile.empty() ? "N/A" : inFile.c_str();
		const auto tooltip_text = inFile.empty() ? "No file loaded." : inFile.c_str();

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

	ImGui::Checkbox("Is Transparent", &inMaterial.isTransparent);
}


void InspectorWidget::DrawComponent(Transform& inTransform)
{
    ImGui::SetNextItemRightAlign("Scale     ");
	if (ImGui::DragVec3("##ScaleDragFloat3", inTransform.scale, 0.001f, 0.0f, FLT_MAX))
		inTransform.Compose();

    ImGui::SetNextItemRightAlign("Rotation");
	auto degrees = glm::degrees(glm::eulerAngles(inTransform.rotation));
	if (ImGui::DragVec3("##RotationDragFloat3", degrees, 0.1f, -360.0f, 360.0f))
	{
		inTransform.rotation = glm::quat(glm::radians(degrees));
		inTransform.Compose();
	}

    ImGui::SetNextItemRightAlign("Position ");
	if (ImGui::DragVec3("##PositionDragFloat3", inTransform.position, 0.001f, -FLT_MAX, FLT_MAX))
		inTransform.Compose();
}


void InspectorWidget::DrawComponent(Light& inLight)
{
	constexpr auto type_names = std::array { "None", "Spot", "Point" };
	ImGui::Combo("Type", (int*)&inLight.type, type_names.data(), type_names.size());

	ImGui::ColorEdit3("Colour", glm::value_ptr(inLight.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	ImGui::DragFloat("Intensity", &inLight.colour.a, 0.001f);

	switch (inLight.type)
	{
		case LIGHT_TYPE_POINT:
		{
			ImGui::DragFloat("Radius", &inLight.attributes.x, 0.001f, 0.0f, 100.0f);
		} break;

		case LIGHT_TYPE_SPOT:
		{
			ImGui::DragFloat("Range", &inLight.attributes.x, 0.001f);
			ImGui::DragFloat("Inner Cone Angle", &inLight.attributes.y, 0.001f, 0.0f, M_PI / 2);
			ImGui::DragFloat("Outer Cone Angle", &inLight.attributes.z, 0.001f, 0.0f, M_PI / 2);
			ImGui::DragFloat3("Direction", &inLight.direction[0], 0.001f);
		} break;
	}
}


void InspectorWidget::DrawComponent(NativeScript& inScript)
{
	auto& scene = GetScene();
	auto& assets = GetAssets();

	if (assets.contains(inScript.file))
	{
		ImGui::Text("File: %s", inScript.file.c_str());
	}

	if (ImGui::Button("Load.."))
	{
		const auto filepath = OS::sOpenFileDialog("DLL Files (*.dll)\0*.dll\0");

		if (!filepath.empty())
		{
			inScript.file = fs::relative(filepath).string();

			if (auto asset = assets.GetAsset<ScriptAsset>(inScript.file))
			{
				for (const auto& type : asset->GetRegisteredTypes())
					inScript.types.push_back(type);
			}
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Reload"))
	{
		assets.Release(inScript.file);
		assets.GetAsset<ScriptAsset>(inScript.file);
		
		scene.BindScriptToEntity(GetActiveEntity(), inScript, m_Editor);
	}

	ImGui::SameLine();

	if (ImGui::Button("Release"))
	{
		assets.Release(inScript.file);
	}

    if (!inScript.types.empty())
    {
        static auto index = -1;

        const auto preview_text = index == -1 ? "None" : inScript.types[index].c_str();

        if (ImGui::BeginCombo("Type", preview_text, ImGuiComboFlags_None))
        {
            for (uint32_t i = 0; i < inScript.types.size(); i++)
            {
                const auto type_name = inScript.types[i].c_str();

                if (ImGui::Selectable(type_name, i == index))
                {
                    index = i;

					inScript.type = type_name;

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
				gForEachTupleElement(std::tuple<int, float, bool, uint32_t>(), [&](auto i)
				{
					if (*member_rtti == RTTI_OF(decltype( i )))
					{
						DrawScriptMember(member->GetCustomName(), member->GetRef< decltype( i )>(inScript.script));
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
}

void InspectorWidget::DrawComponent(DDGISceneSettings& ioSettings)
{
	auto& scene = GetScene();

	if (ImGui::Button("Fit to Scene", ImVec2(ImGui::GetWindowWidth(), 0)))
	{
		ioSettings.FitToScene(scene, scene.Get<Transform>(GetActiveEntity()));
	}

	ImGui::DragInt3("Probe Count", glm::value_ptr(ioSettings.mDDGIProbeCount), 1, 1, 40);
	ImGui::DragFloat3("Probe Spacing", glm::value_ptr(ioSettings.mDDGIProbeSpacing), 0.01f, -1000.0f, 1000.0f, "%.3f");
}


void InspectorWidget::DrawComponent(DirectionalLight& inDirectionalLight)
{
	ImGui::AlignTextToFramePadding();
	ImGui::Text("Colour     ");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::ColorEdit3("##Colour", glm::value_ptr(inDirectionalLight.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

	ImGui::AlignTextToFramePadding();
	ImGui::Text("Intensity  ");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::DragFloat("##Intensity", &inDirectionalLight.colour.a, 0.001f, 0.0f, FLT_MAX);
}

}