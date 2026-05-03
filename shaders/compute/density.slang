#include "../defines.slang"

struct FluidParticle {
  float3 position;
  float3 velocity;
  float3 distance_traveled;
  float4 color;
  float density;
};

struct WallParticle {
  float3 position;
};

struct UniformBuffer {
  uint voxel_max_particles;
  uint fluid_particle_count;
  uint wall_particle_count;
  uint total_particle_count;
  float rest_density;
  float particle_mass;
  float effective_radius;
  float effective_radius_2;
  float effective_radius_6;
  float effective_radius_9;
  float viscosity;
  float gas_constant;
  float damping_coefficient;
  float mu;
  float yield_stress;
  uint elevation_width;
  uint elevation_height;
  uint elevation_padding;
  uint3 bucket_size;
  float3 min_bound;
  float3 max_bound;
};
ConstantBuffer<UniformBuffer> ubo;

struct FluidParticleSSBO {
  FluidParticle fluid_particle;
};

struct BucketSSBO {
  uint element;
};

StructuredBuffer<FluidParticleSSBO> fluid_particles_in;
RWStructuredBuffer<FluidParticleSSBO> fluid_particles_out;
StructuredBuffer<BucketSSBO> bucket_in;

float w(float3 r) {
  const float c = 315.0 / (64.0 * PI);
  float abs_r2 = dot(r, r);
  float diff_r2 = ubo.effective_radius_2 - abs_r2;
  return 0 < diff_r2 ? c * POW3(diff_r2) / ubo.effective_radius_9 : 0.0;
}

[shader("compute")]
[numthreads(NUM_THREADS, 1, 1)]
void comp_main(uint3 thread_id: SV_DispatchThreadID) {
  uint index = thread_id.x;

  if (index >= ubo.fluid_particle_count) {
    return;
  }

  float3 position = fluid_particles_in[index].fluid_particle.position;
  int3 bucket_position =
      int3((position - ubo.min_bound) / (ubo.max_bound - ubo.min_bound) * (ubo.bucket_size - 1));

  int neighbor_count = 0;

  float rho = 0.0;
  for (int x_offset = -1; x_offset <= 1; x_offset++) {
    for (int y_offset = -1; y_offset <= 1; y_offset++) {
      for (int z_offset = -1; z_offset <= 1; z_offset++) {
        int3 neighbor_bucket_position = bucket_position + int3(x_offset, y_offset, z_offset);
        if (any(neighbor_bucket_position < 0) ||
            any((uint3)neighbor_bucket_position >= ubo.bucket_size))
          continue;

        uint neighbor_index = neighbor_bucket_position.x +
                              neighbor_bucket_position.y * ubo.bucket_size.x +
                              neighbor_bucket_position.z * ubo.bucket_size.x * ubo.bucket_size.y;
        neighbor_index *= ubo.voxel_max_particles;
        for (uint i = 0; i < ubo.voxel_max_particles; i++) {
          uint j = bucket_in[neighbor_index + i].element;
          if (j == ubo.total_particle_count)
            break;

          float3 neighbor_position;
          if (j < ubo.fluid_particle_count) {
            neighbor_position = fluid_particles_in[j].fluid_particle.position;
            rho += ubo.particle_mass * w(position - neighbor_position);

            neighbor_count++;
          }
        }
      }
    }
  }

  fluid_particles_out[index].fluid_particle = fluid_particles_in[index].fluid_particle;

  if (neighbor_count > 0)
    fluid_particles_out[index].fluid_particle.density = rho;
  else
    fluid_particles_out[index].fluid_particle.density = ubo.rest_density;
}

