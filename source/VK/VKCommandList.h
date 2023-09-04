#pragma once

namespace Raekor::VK {

class Texture;

class CommandList
{
public:
	void ImageLayoutBarrier(const Texture& texture, VkImageLayout oldLayout, VkImageLayout newLayout);

private:
	VkCommandBuffer commands;

};

}
