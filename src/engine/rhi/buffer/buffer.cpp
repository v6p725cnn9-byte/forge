#include "engine/rhi/buffer/buffer.hpp"

#include "engine/rhi/command/command.hpp"

#include <cstring>

namespace forge::rhi {

SDL_GPUBuffer* create_buffer(SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage, std::uint32_t size,
                             const void* bytes)
{
    if (!device || size == 0) return nullptr;
    SDL_GPUBufferCreateInfo info{};
    info.usage = usage;
    info.size = size;
    auto* buffer = SDL_CreateGPUBuffer(device, &info);
    if (!buffer || !bytes) return buffer;

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = size;
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!transfer) {
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }
    std::memcpy(mapped, bytes, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    Command command(device);
    bool ok = false;
    if (command.handle) {
        if (auto* pass = SDL_BeginGPUCopyPass(command.handle)) {
            SDL_GPUTransferBufferLocation src{};
            src.transfer_buffer = transfer;
            SDL_GPUBufferRegion dst{};
            dst.buffer = buffer;
            dst.size = size;
            SDL_UploadToGPUBuffer(pass, &src, &dst, false);
            SDL_EndGPUCopyPass(pass);
            ok = command.submit();
        }
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (!ok) {
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }
    return buffer;
}

void destroy_buffer(SDL_GPUDevice* device, SDL_GPUBuffer*& buffer)
{
    if (device && buffer) SDL_ReleaseGPUBuffer(device, buffer);
    buffer = nullptr;
}

} // namespace forge::rhi
