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

struct PushConstants {
  float time_step;
};

[[vk::push_constant]]
PushConstants push_constants;

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
  float min_elevation;
  float max_elevation;
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

struct WallParticleSSBO {
  WallParticle wall_particle;
};

struct BucketSSBO {
  uint element;
};

struct Elevation {
  float2 uv;
  float elevation;
};

StructuredBuffer<WallParticleSSBO> wall_particles_in;
StructuredBuffer<FluidParticleSSBO> fluid_particles_in;
RWStructuredBuffer<FluidParticleSSBO> fluid_particles_out;
StructuredBuffer<BucketSSBO> bucket_in;
StructuredBuffer<Elevation> elevation_buffer;

float3x3 calculate_strain_rate_tensor(float3 vel_i, float3 neighbor_velocity, float3 r_ij) {
  float inv_r = 1.0 / length(r_ij);
  float3 vel_diff = neighbor_velocity - vel_i;

  float3x3 grad_v;
  grad_v[0] = vel_diff * (r_ij.x * inv_r);
  grad_v[1] = vel_diff * (r_ij.y * inv_r);
  grad_v[2] = vel_diff * (r_ij.z * inv_r);

  float3x3 D;
  D[0][0] = grad_v[0][0];
  D[0][1] = 0.5 * (grad_v[0][1] + grad_v[1][0]);
  D[0][2] = 0.5 * (grad_v[0][2] + grad_v[2][0]);
  D[1][0] = D[0][1];
  D[1][1] = grad_v[1][1];
  D[1][2] = 0.5 * (grad_v[1][2] + grad_v[2][1]);
  D[2][0] = D[0][2];
  D[2][1] = D[1][2];
  D[2][2] = grad_v[2][2];

  return D;
}

float compute_D_magnitude(float3x3 D) {
  // Calculate |D| = sqrt(D:D) - the second invariant of D
  float sum = 0.0;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      sum += D[i][j] * D[i][j];
    }
  }
  return sqrt(sum);
}

float3x3 calculate_stress_tensor(float3x3 D, float D_mag, float viscosity) {
  float3x3 tau;

  if (D_mag < epsilon) {
    // Zero strain rate - treat as solid
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        tau[i][j] = 0.0;
      }
    }
  } else {
    float bingham_factor = ubo.yield_stress / D_mag + 2.0 * viscosity;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        tau[i][j] = bingham_factor * D[i][j];
      }
    }
  }

  return tau;
}

float3 grad_W_press(float3 r) {
  const float c = 45.0 / PI;
  float abs_r = length(r);
  float diff_r = ubo.effective_radius - abs_r;
  return (epsilon < diff_r) && (epsilon < abs_r)
             ? (c / ubo.effective_radius_6) * POW3(diff_r) * r / abs_r
             : 0.0;
}

float grad_W_vis(float3 r) {
  const float c = 45.0 / PI;
  float abs_r = length(r);
  return abs_r < ubo.effective_radius
             ? (c / ubo.effective_radius_6) * (ubo.effective_radius - abs_r)

             : 0.0;
}

float sample_elevation(uint x, uint y) {
  uint index = y * ubo.elevation_width + x;
  return elevation_buffer[index].elevation;
}

float get_elevation(float3 pos) {
  if (ubo.elevation_width == 0 || ubo.elevation_height == 0) {
    return ubo.min_elevation;
  }

  // Convert world position to UV coordinates (0-1 range)
  float2 uv = (pos.xz - ubo.min_bound.xz) / (ubo.max_bound.xz - ubo.min_bound.xz);

  // Clamp UV coordinates to ensure we're within the texture bounds
  uv = clamp(uv, float2(0, 0), float2(1, 1));

  float scaled_x = uv.x * max(1.0, float(ubo.elevation_width - 1));
  float scaled_y = uv.y * max(1.0, float(ubo.elevation_height - 1));

  uint x0 = min((uint)floor(scaled_x), ubo.elevation_width - 1);
  uint y0 = min((uint)floor(scaled_y), ubo.elevation_height - 1);
  uint x1 = min(x0 + 1, ubo.elevation_width - 1);
  uint y1 = min(y0 + 1, ubo.elevation_height - 1);

  float tx = saturate(scaled_x - float(x0));
  float ty = saturate(scaled_y - float(y0));

  float e00 = sample_elevation(x0, y0);
  float e10 = sample_elevation(x1, y0);
  float e01 = sample_elevation(x0, y1);
  float e11 = sample_elevation(x1, y1);

  return lerp(lerp(e00, e10, tx), lerp(e01, e11, tx), ty);
}

float3 get_surface_normal(float3 pos) {
  // Small offset for gradient computation
  const float h = ubo.effective_radius * 0.5;

  // Sample elevations at offset positions
  float elevX1 = get_elevation(pos + float3(h, 0, 0));
  float elevX2 = get_elevation(pos - float3(h, 0, 0));
  float elevZ1 = get_elevation(pos + float3(0, 0, h));
  float elevZ2 = get_elevation(pos - float3(0, 0, h));

  // Compute gradient (partial derivatives)
  float dydx = (elevX1 - elevX2) / (2.0 * h);
  float dydz = (elevZ1 - elevZ2) / (2.0 * h);

  // Normal is perpendicular to the gradient
  float3 normal = normalize(float3(-dydx, 1.0, -dydz));
  return normal;
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

  float density = fluid_particles_in[index].fluid_particle.density;
  float pressure = ubo.gas_constant * (density - ubo.rest_density);
  float3 velocity_in = fluid_particles_in[index].fluid_particle.velocity;

  float3 f_press = 0.0;
  float3 f_vis = 0.0;
  float3 f_bound = 0.0;

  bool is_isolated = false;
  int neighborCount = 0;

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
          if (j == index)
            continue;

          if (j >= ubo.fluid_particle_count) {
            j -= ubo.fluid_particle_count;
            float3 neighbor_position = wall_particles_in[j].wall_particle.position;
            float3 from_neighbor = position - neighbor_position;
            float dist = length(from_neighbor);
            float pen_dist = ubo.effective_radius - dist;

            float3 normal = get_surface_normal(position);
            float d_dummy = ubo.effective_radius;
            float d_soil = dist;

            float beta_prime = d_dummy / max(d_soil, epsilon);

            float3 delta_v_i = -velocity_in.xyz;

            if (pen_dist > epsilon && dot(velocity_in.xyz, normal) < 0.0) {
              float3 delta_v_n_i = dot(delta_v_i, normal) * normal;
              float3 modified_normal_vel =
                  beta_prime * (1.0 - ubo.damping_coefficient) * delta_v_n_i;
              float3 delta_v_s_i = delta_v_i - delta_v_n_i;

              float f_n_magnitude = length((2.0 * ubo.particle_mass * modified_normal_vel) /
                                           push_constants.time_step);
              float f_s_magnitude =
                  length((2.0 * ubo.particle_mass * delta_v_s_i) / push_constants.time_step);

              if (f_s_magnitude > ubo.mu * f_n_magnitude && f_n_magnitude > epsilon)
                delta_v_s_i *= ubo.mu * (f_n_magnitude / f_s_magnitude);

              float3 f_normal =
                  (2.0 * ubo.particle_mass * modified_normal_vel) / push_constants.time_step;
              float3 f_tangential =
                  (2.0 * ubo.particle_mass * delta_v_s_i) / push_constants.time_step;

              f_bound += f_normal + f_tangential;
            }
          } else {
            float neighbor_density = fluid_particles_in[j].fluid_particle.density;
            float neighbor_pressure = ubo.gas_constant * (neighbor_density - ubo.rest_density);
            float3 neighbor_position = fluid_particles_in[j].fluid_particle.position;
            float3 from_neighbor = position - neighbor_position;
            float3 neighbor_velocity = fluid_particles_in[j].fluid_particle.velocity;

            // Prevent division by zero in force calculations
            if (neighbor_density > epsilon) {
              f_press += ((pressure + neighbor_pressure) / (2.0 * neighbor_density)) *
                         grad_W_press(from_neighbor);

              float dist = length(from_neighbor);
              if (dist < ubo.effective_radius) {
                float3x3 D =
                    calculate_strain_rate_tensor(velocity_in.xyz, neighbor_velocity, from_neighbor);
                float D_mag = compute_D_magnitude(D);

                float3x3 tau = calculate_stress_tensor(D, D_mag, ubo.viscosity);

                float3 force_contribution = 0.0;

                force_contribution += tau[0] * normalize(from_neighbor)[0];
                force_contribution += tau[1] * normalize(from_neighbor)[1];
                force_contribution += tau[2] * normalize(from_neighbor)[2];

                f_vis += (force_contribution / neighbor_density) * grad_W_vis(from_neighbor);
              }

              if (dist < ubo.effective_radius * 1.5)
                neighborCount++;
            }
          }
        }
      }
    }
  }

  is_isolated = neighborCount < 3;

  // Prevent division by zero in final force calculations
  if (density > epsilon) {
    f_press *= ubo.particle_mass * ubo.particle_mass / density;
    f_vis *= ubo.viscosity * ubo.particle_mass * ubo.particle_mass / density;
  }

  float3 acceleration = a_gravity + (float3(f_press + f_vis) + f_bound) / ubo.particle_mass;
  float3 velocity_out = velocity_in + float3(acceleration * push_constants.time_step);

  // Maximum allowed velocity to prevent numerical issues
  const float max_speed = 15.0;  // Adjust based on your scene scale
  float speed = length(velocity_out.xyz);

  if (speed > max_speed) {
    // For isolated or high-speed particles, apply stronger damping
    float dampingFactor = is_isolated ? 0.95 : 0.9;
    velocity_out.xyz *= dampingFactor;

    // // If the particle is truly isolated, try to reintegrate it
    // if (is_isolated && density < 0.7 * ubo.rest_density) {
    //     // Apply velocity towards center of simulation to try pulling it back
    //     float3 toCenter = (ubo.min_bound + ubo.max_bound) * 0.5 - position;
    //     float centerDist = length(toCenter);
    //     if (centerDist > 0.001) {
    //         toCenter = normalize(toCenter);
    //         velocity_out.xyz += toCenter * push_constants.time_step * 2.0;
    //     }
    // }
  }

  float3 position_out = position + push_constants.time_step * velocity_out;

  float elevation = get_elevation(position_out.xyz);
  position_out.y = max(position_out.y, elevation);

  position_out = clamp(position_out, ubo.min_bound, ubo.max_bound);

  fluid_particles_out[index].fluid_particle = fluid_particles_in[index].fluid_particle;

  fluid_particles_out[index].fluid_particle.distance_traveled += position_out.xyz - position;

  fluid_particles_out[index].fluid_particle.velocity = velocity_out;
  fluid_particles_out[index].fluid_particle.position = position_out;
}

