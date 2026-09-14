#include "engine/rhi/device/upload.hpp"

#include "engine/rhi/command/command.hpp"

#include <cstring>

namespace forge::rhi {

bool upload_texture_bytes(SDL_GPUDevice* device, SDL_GPUTexture* texture, const void* bytes, std::uint32_t size,
                          const std::vector<TextureBlit>& slices, bool generate_mips)
{
    if (!device || !texture || !bytes || size == 0) return false;
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = size;
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!transfer) return false;
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    std::memcpy(mapped, bytes, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    Command command(device);
    bool success = false;
    if (command.handle) {
        if (auto* pass = SDL_BeginGPUCopyPass(command.handle)) {
            for (const auto& slice : slices) {
                SDL_GPUTextureTransferInfo src{};
                src.transfer_buffer = transfer;
                src.offset = slice.offset;
                SDL_GPUTextureRegion dst{};
                dst.texture = texture;
                dst.mip_level = slice.level;
                dst.layer = slice.face;
                dst.w = slice.width;
                dst.h = slice.height;
                dst.d = 1;
                SDL_UploadToGPUTexture(pass, &src, &dst, false);
            }
            SDL_EndGPUCopyPass(pass);
            if (generate_mips) SDL_GenerateMipmapsForGPUTexture(command.handle, texture);
            success = command.submit();
        }
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return success;
}

} // namespace forge::rhi
