/* vk_pipe.c - create one compute pipeline from a SPIR-V file, nothing else.
 *
 *   vk_pipe <shader.spv> [gpu-name-substring]
 *
 * Vulkan 1.3 device with bufferDeviceAddress enabled, pipeline layout =
 * set 0 with storage buffers at bindings 0..3, entry point "main".
 *
 * Build (Windows, VS developer prompt, Vulkan SDK installed):
 *   cl /nologo /W3 vk_pipe.c /I"%VULKAN_SDK%\Include" /link /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib
 * Build (Linux):
 *   cc -o vk_pipe vk_pipe.c -lvulkan
 */
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { \
	fprintf(stderr, "%s failed: %d\n", #x, (int)r_); return 1; } } while (0)

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <shader.spv> [gpu-name-substring]\n", argv[0]);
		return 2;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) { perror(argv[1]); return 1; }
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint32_t *code = malloc((size_t)size);
	if (size <= 0 || size % 4 || fread(code, 1, (size_t)size, f) != (size_t)size) {
		fprintf(stderr, "bad SPIR-V file\n");
		return 1;
	}
	fclose(f);

	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_3;
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	VkInstance inst;
	CHECK(vkCreateInstance(&ici, NULL, &inst));

	uint32_t n = 16;
	VkPhysicalDevice pds[16], pd = VK_NULL_HANDLE;
	vkEnumeratePhysicalDevices(inst, &n, pds);
	for (uint32_t i = 0; i < n; i++) {
		VkPhysicalDeviceDriverProperties drv = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
		VkPhysicalDeviceProperties2 p = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &drv };
		vkGetPhysicalDeviceProperties2(pds[i], &p);
		int pick = !pd && (argc < 3 || strstr(p.properties.deviceName, argv[2]));
		printf("%s %s | %s | %s | driverVersion 0x%x\n", pick ? "*" : " ", p.properties.deviceName,
			drv.driverName, drv.driverInfo, p.properties.driverVersion);
		if (pick)
			pd = pds[i];
	}
	if (!pd) { fprintf(stderr, "no matching device\n"); return 1; }

	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, NULL);
	VkQueueFamilyProperties qf[32];
	if (nq > 32) nq = 32;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qf);
	uint32_t family = 0;
	while (family < nq && !(qf[family].queueFlags & VK_QUEUE_COMPUTE_BIT))
		family++;

	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qci.queueFamilyIndex = family;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;

	VkPhysicalDeviceVulkan12Features f12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	f12.bufferDeviceAddress = VK_TRUE;
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f12 };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	VkDevice dev;
	CHECK(vkCreateDevice(pd, &dci, NULL, &dev));

	VkDescriptorSetLayoutBinding b[4];
	for (uint32_t i = 0; i < 4; i++) {
		VkDescriptorSetLayoutBinding bi = { i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		b[i] = bi;
	}
	VkDescriptorSetLayoutCreateInfo dslci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	dslci.bindingCount = 4;
	dslci.pBindings = b;
	VkDescriptorSetLayout dsl;
	CHECK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl));

	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &dsl;
	VkPipelineLayout layout;
	CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &layout));

	VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	smci.codeSize = (size_t)size;
	smci.pCode = code;
	VkShaderModule sm;
	CHECK(vkCreateShaderModule(dev, &smci, NULL, &sm));

	VkComputePipelineCreateInfo cpci = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpci.stage.module = sm;
	cpci.stage.pName = "main";
	cpci.layout = layout;
	cpci.basePipelineIndex = -1;

	printf("vkCreateComputePipelines...\n");
	fflush(stdout);
	VkPipeline pipe;
	CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipe));
	printf("OK\n");

	vkDestroyPipeline(dev, pipe, NULL);
	vkDestroyShaderModule(dev, sm, NULL);
	vkDestroyPipelineLayout(dev, layout, NULL);
	vkDestroyDescriptorSetLayout(dev, dsl, NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroyInstance(inst, NULL);
	free(code);
	return 0;
}
