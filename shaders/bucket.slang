#include "defines.slang"

struct FluidParticle {
  float3 position;
  float3 velocity;
  float3 distance_traveled;
  float4 color;
};

struct WallParticle {
  float3 position;
};

struct UniformBuffer {
  uint voxel_max_particles;
  uint fluid_particle_count;
  uint wall_particle_count;
  uint total_particle_count;
  uint3 bucket_size;
  float3 min_bound;
  float3 max_bound;
};
ConstantBuffer<UniformBuffer> ubo;

struct FluidParticleSSBO {
  FluidParticle fluid_particle;
};

struct WallParticleSSBO {
  WallParticle wall_particle;
};

struct BucketSSBO {
  uint element;
};

StructuredBuffer<FluidParticleSSBO> fluid_particles_in;
StructuredBuffer<WallParticleSSBO> wall_particles_in;

RWStructuredBuffer<BucketSSBO> bucket_out;

[shader("compute")]
[numthreads(NUM_THREADS, 1, 1)]
void fluid_bucket(uint3 thread_id: SV_DispatchThreadID) {
  uint index = thread_id.x;

  if (index >= ubo.fluid_particle_count) {
    return;
  }

  float3 position = fluid_particles_in[index].fluid_particle.position;
  float3 bucket_position =
      int3((position - ubo.min_bound) / (ubo.max_bound - ubo.min_bound) * (ubo.bucket_size - 1));
  uint bucket_index = uint(bucket_position.x + bucket_position.y * ubo.bucket_size.x +
                           bucket_position.z * ubo.bucket_size.x * ubo.bucket_size.y);
  bucket_index *= ubo.voxel_max_particles;
  for (uint j = 0; j < ubo.voxel_max_particles; j++) {
    uint original;
    InterlockedCompareExchange(bucket_out[bucket_index + j].element, ubo.total_particle_count,
                               index, original);
    if (original == ubo.total_particle_count)
      break;
  }
}

[shader("compute")]
[numthreads(NUM_THREADS, 1, 1)]
void wall_bucket(uint3 thread_id: SV_DispatchThreadID) {
  uint index = thread_id.x;

  if (index >= ubo.wall_particle_count) {
    return;
  }

  float3 position = wall_particles_in[index].wall_particle.position;
  float3 bucket_position =
      int3((position - ubo.min_bound) / (ubo.max_bound - ubo.min_bound) * (ubo.bucket_size - 1));
  uint bucket_index = uint(bucket_position.x + bucket_position.y * ubo.bucket_size.x +
                           bucket_position.z * ubo.bucket_size.x * ubo.bucket_size.y);
  bucket_index *= ubo.voxel_max_particles;
  for (uint j = 0; j < ubo.voxel_max_particles; j++) {
    uint original;
    InterlockedCompareExchange(bucket_out[bucket_index + j].element, ubo.total_particle_count,
                               index, original);
    if (original == ubo.total_particle_count)
      break;
  }
}

[shader("compute")]
[numthreads(CLEAR_BUCKETS_NUM_THREADS, CLEAR_BUCKETS_NUM_THREADS, CLEAR_BUCKETS_NUM_THREADS)]
void clear_bucket(uint3 thread_id: SV_DispatchThreadID) {
  if (all(thread_id >= ubo.bucket_size)) {
    return;
  }

  uint bucket_index = uint(thread_id.x + thread_id.y * ubo.bucket_size.x +
                           thread_id.z * ubo.bucket_size.x * ubo.bucket_size.y);
  bucket_index *= ubo.voxel_max_particles;
  for (uint j = 0; j < ubo.voxel_max_particles; j++) {
    bucket_out[bucket_index + j].element = ubo.total_particle_count;
  }
}
