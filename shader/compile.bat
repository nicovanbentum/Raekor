%VULKAN_SDK%/Bin32/glslc.exe shader.vert -o vert.spv
%VULKAN_SDK%/Bin32/glslc.exe shader.frag -o frag.spv
%VULKAN_SDK%/Bin32/glslc.exe triangle.vert -o triangleVert.spv
%VULKAN_SDK%/Bin32/glslc.exe triangle.frag -o triangleFrag.spv
%VULKAN_SDK%/Bin32/glslc.exe raytrace.rmiss -o raytrace.rmiss.spv
%VULKAN_SDK%/Bin32/glslc.exe raytrace.rgen -o raytrace.rgen.spv
%VULKAN_SDK%/Bin32/glslc.exe raytrace.rchit -o raytrace.rchit.spv
pause