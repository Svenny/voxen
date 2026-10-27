/* vk_store.c - run a compute shader that copies 64-bit values from one storage buffer
 * to another, then check the copy.
 *
 *   vk_store <shader.spv> [gpu-name-substring]
 *
 * Vulkan 1.3 device with bufferDeviceAddress and shaderInt64 enabled. Pipeline layout = set 0
 * with storage buffers at binding 0 (input, N uint64 values) and binding 1 (output, N 8-byte
 * slots, zero-filled before the dispatch). Entry point "main", local size 64, N = 256.
 * The shader must write input[i] into output slot i. Values are arbitrary 64-bit patterns;
 * in the pointer variants they are only stored, never dereferenced.
 *
 * Build (Windows, VS developer prompt, Vulkan SDK installed):
 *   cl /nologo /W3 vk_store.c /I"%VULKAN_SDK%\Include" /link /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib
 * Build (Linux):
 *   cc -o vk_store vk_store.c -lvulkan
 */
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 256u

#define CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { \
	fprintf(stderr, "%s failed: %d\n", #x, (int)r_); exit(1); } } while (0)

static VkDevice dev;
static VkPhysicalDeviceMemoryProperties memprops;

static VkBuffer makeBuffer(VkDeviceSize size, void **mapped)
{
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VkBuffer buf;
	CHECK(vkCreateBuffer(dev, &bci, NULL, &buf));
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, buf, &req);
	VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uint32_t type = 0;
	while (type < memprops.memoryTypeCount
		&& (!(req.memoryTypeBits & (1u << type)) || (memprops.memoryTypes[type].propertyFlags & want) != want))
		type++;
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = type;
	VkDeviceMemory mem;
	CHECK(vkAllocateMemory(dev, &mai, NULL, &mem));
	CHECK(vkBindBufferMemory(dev, buf, mem, 0));
	CHECK(vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, mapped));
	memset(*mapped, 0, (size_t)size);
	return buf;
}

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
	vkGetPhysicalDeviceMemoryProperties(pd, &memprops);

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
	VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f12 };
	f2.features.shaderInt64 = VK_TRUE;
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2 };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	CHECK(vkCreateDevice(pd, &dci, NULL, &dev));
	VkQueue queue;
	vkGetDeviceQueue(dev, family, 0, &queue);

	uint64_t *in_data, *out_data;
	VkBuffer in = makeBuffer(N * 8, (void **)&in_data);
	VkBuffer out = makeBuffer(N * 8, (void **)&out_data);
	for (uint32_t i = 0; i < N; i++)
		in_data[i] = 0x0000000312340000ull + (uint64_t)i * 0x40;

	VkDescriptorSetLayoutBinding b[2] = {
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL },
	};
	VkDescriptorSetLayoutCreateInfo dslci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	dslci.bindingCount = 2;
	dslci.pBindings = b;
	VkDescriptorSetLayout dsl;
	CHECK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl));

	VkDescriptorPoolSize ps = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
	VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &ps;
	VkDescriptorPool pool;
	CHECK(vkCreateDescriptorPool(dev, &dpci, NULL, &pool));
	VkDescriptorSetAllocateInfo dsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dsai.descriptorPool = pool;
	dsai.descriptorSetCount = 1;
	dsai.pSetLayouts = &dsl;
	VkDescriptorSet set;
	CHECK(vkAllocateDescriptorSets(dev, &dsai, &set));

	VkDescriptorBufferInfo dbi[2] = { { in, 0, N * 8 }, { out, 0, N * 8 } };
	VkWriteDescriptorSet w = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	w.dstSet = set;
	w.descriptorCount = 2;
	w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	w.pBufferInfo = dbi;
	vkUpdateDescriptorSets(dev, 1, &w, 0, NULL);

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
	VkPipeline pipe;
	CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipe));

	VkCommandPoolCreateInfo cpi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpi.queueFamilyIndex = family;
	VkCommandPool cmdpool;
	CHECK(vkCreateCommandPool(dev, &cpi, NULL, &cmdpool));
	VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cbai.commandPool = cmdpool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	VkCommandBuffer cb;
	CHECK(vkAllocateCommandBuffers(dev, &cbai, &cb));
	VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	CHECK(vkBeginCommandBuffer(cb, &cbbi));
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
	vkCmdDispatch(cb, N / 64, 1, 1);
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0,
		NULL);
	CHECK(vkEndCommandBuffer(cb));
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
	CHECK(vkQueueWaitIdle(queue));

	uint32_t bad = 0;
	for (uint32_t i = 0; i < N; i++) {
		if (out_data[i] != in_data[i]) {
			if (bad < 4)
				printf("  out[%u] = 0x%016llx, expected 0x%016llx\n", i, (unsigned long long)out_data[i],
					(unsigned long long)in_data[i]);
			bad++;
		}
	}
	printf("%u of %u values wrong: %s\n", bad, N, bad ? "FAIL" : "PASS");

	vkDestroyPipeline(dev, pipe, NULL);
	vkDestroyShaderModule(dev, sm, NULL);
	vkDestroyPipelineLayout(dev, layout, NULL);
	vkDestroyCommandPool(dev, cmdpool, NULL);
	vkDestroyDescriptorPool(dev, pool, NULL);
	vkDestroyDescriptorSetLayout(dev, dsl, NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroyInstance(inst, NULL);
	free(code);
	return bad ? 3 : 0;
}
