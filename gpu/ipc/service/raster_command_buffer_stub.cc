// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/raster_command_buffer_stub.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/raster_decoder.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_workarounds.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif

namespace gpu {

RasterCommandBufferStub::RasterCommandBufferStub(
    GpuChannel* channel,
    const GPUCreateCommandBufferConfig& init_params,
    CommandBufferId command_buffer_id,
    SequenceId sequence_id,
    int32_t stream_id,
    int32_t route_id)
    : CommandBufferStub(channel,
                        init_params,
                        command_buffer_id,
                        sequence_id,
                        stream_id,
                        route_id) {}

RasterCommandBufferStub::~RasterCommandBufferStub() {}

gpu::ContextResult RasterCommandBufferStub::Initialize(
    CommandBufferStub* share_command_buffer_stub,
    const GPUCreateCommandBufferConfig& init_params,
    base::UnsafeSharedMemoryRegion shared_state_shm) {
  TRACE_EVENT0("gpu", "RasterBufferStub::Initialize");
  FastSetActiveURL(active_url_, active_url_hash_, channel_);

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  if (share_command_buffer_stub) {
    LOG(ERROR) << "Using a share group is not supported with RasterDecoder";
    return ContextResult::kFatalFailure;
  }

  if (surface_handle_ != kNullSurfaceHandle) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "RasterInterface clients must render offscreen.";
    return ContextResult::kFatalFailure;
  }

  if (init_params.attribs.gpu_preference != gl::PreferIntegratedGpu ||
      init_params.attribs.context_type != CONTEXT_TYPE_OPENGLES2 ||
      init_params.attribs.bind_generates_resource) {
    LOG(ERROR) << "ContextResult::kFatalFailure: Incompatible creation attribs "
                  "used with RasterDecoder";
    return ContextResult::kFatalFailure;
  }

  ContextResult result;
  auto raster_decoder_context_state =
      manager->GetRasterDecoderContextState(&result);
  if (!raster_decoder_context_state) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Failed to create raster decoder state.";
    DCHECK_NE(result, gpu::ContextResult::kSuccess);
    return result;
  }

  if (!raster_decoder_context_state->IsGLInitialized()) {
    if (!raster_decoder_context_state->MakeCurrent(nullptr) ||
        !raster_decoder_context_state->InitializeGL(
            base::MakeRefCounted<gles2::FeatureInfo>(
                manager->gpu_driver_bug_workarounds(),
                manager->gpu_feature_info()))) {
      LOG(ERROR) << "Failed to Initialize GL for RasterDecoderContextState";
      return ContextResult::kFatalFailure;
    }
  }

  gpu::GpuMemoryBufferFactory* gmb_factory =
      manager->gpu_memory_buffer_factory();
  context_group_ = base::MakeRefCounted<gles2::ContextGroup>(
      manager->gpu_preferences(), gles2::PassthroughCommandDecoderSupported(),
      manager->mailbox_manager(), CreateMemoryTracker(init_params),
      manager->shader_translator_cache(),
      manager->framebuffer_completeness_cache(),
      raster_decoder_context_state->feature_info(),
      init_params.attribs.bind_generates_resource, channel_->image_manager(),
      gmb_factory ? gmb_factory->AsImageFactory() : nullptr,
      /*progress_reporter=*/manager->watchdog(), manager->gpu_feature_info(),
      manager->discardable_manager(),
      manager->passthrough_discardable_manager(),
      manager->shared_image_manager());

  surface_ = raster_decoder_context_state->surface();
  share_group_ = raster_decoder_context_state->share_group();
  use_virtualized_gl_context_ =
      raster_decoder_context_state->use_virtualized_gl_contexts;

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, context_group_->transfer_buffer_manager());
  std::unique_ptr<raster::RasterDecoder> decoder(raster::RasterDecoder::Create(
      this, command_buffer_.get(), manager->outputter(), context_group_.get(),
      raster_decoder_context_state));

  sync_point_client_state_ =
      channel_->sync_point_manager()->CreateSyncPointClientState(
          CommandBufferNamespace::GPU_IO, command_buffer_id_, sequence_id_);

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");

  scoped_refptr<gl::GLContext> context =
      raster_decoder_context_state->context();
  if (!raster_decoder_context_state->MakeCurrent(nullptr)) {
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Failed to make context current.";
    return gpu::ContextResult::kTransientFailure;
  }

  if (!context_group_->has_program_cache() &&
      !context_group_->feature_info()->workarounds().disable_program_cache) {
    context_group_->set_program_cache(manager->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  result = decoder->Initialize(surface_, context, true /* offscreen */,
                               gpu::gles2::DisallowedFeatures(),
                               init_params.attribs);
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    return result;
  }

  if (manager->gpu_preferences().enable_gpu_service_logging) {
    decoder->SetLogCommands(true);
  }
  set_decoder_context(std::move(decoder));

  const size_t kSharedStateSize = sizeof(CommandBufferSharedState);
  base::WritableSharedMemoryMapping shared_state_mapping =
      shared_state_shm.MapAt(0, kSharedStateSize);
  if (!shared_state_mapping.IsValid()) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Failed to map shared state buffer.";
    return gpu::ContextResult::kFatalFailure;
  }
  command_buffer_->SetSharedStateBuffer(MakeBackingFromSharedMemory(
      std::move(shared_state_shm), std::move(shared_state_mapping)));

  if (!active_url_.is_empty())
    manager->delegate()->DidCreateOffscreenContext(active_url_);

  manager->delegate()->DidCreateContextSuccessfully();
  initialized_ = true;
  return gpu::ContextResult::kSuccess;
}

// RasterInterface clients should not manipulate the front buffer.
void RasterCommandBufferStub::OnTakeFrontBuffer(const Mailbox& mailbox) {
  NOTREACHED();
}
void RasterCommandBufferStub::OnReturnFrontBuffer(const Mailbox& mailbox,
                                                  bool is_lost) {
  NOTREACHED();
}

void RasterCommandBufferStub::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

void RasterCommandBufferStub::SetActiveURL(GURL url) {
  active_url_ = std::move(url);
  active_url_hash_ = base::Hash(active_url_.possibly_invalid_spec());
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
}

}  // namespace gpu
