struct CameraUBO {
  float4x4 model;
  float4x4 view;
  float4x4 proj;
  uint has_terrain_texture;
};

ConstantBuffer<CameraUBO> camera;

Sampler2D terrainTexture;

struct VSInput {
  float2 in_uv;
  float in_elevation;
  float3 in_position;
};

struct VSOutput {
  float4 pos : SV_Position;
  float point_size : SV_PointSize;
  float2 in_uv : TEXCOORD0;
};

struct PSInput {
  float4 pos : SV_Position;
  float2 in_uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vert_main(VSInput input) {
  VSOutput output;
  float4 worldPos = mul(camera.model, float4(input.in_position, 1.0));
  float4 viewPos = mul(camera.view, worldPos);
  output.pos = mul(camera.proj, viewPos);
  output.in_uv = input.in_uv;
  return output;
}

[shader("fragment")]
float4 frag_main(PSInput input) : SV_Target {
  // Only sample and show the terrain texture if the UI/renderer indicated it
  // is present. Otherwise return a neutral color.
  if (camera.has_terrain_texture != 0u) {
    return terrainTexture.Sample(input.in_uv);
  }
  return float4(0.8, 0.8, 0.8, 1.0);
}
