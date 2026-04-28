struct CameraUBO {
  float4x4 model;
  float4x4 view;
  float4x4 proj;
};

ConstantBuffer<CameraUBO> camera;

struct VSInput {
  float2 in_uv;
  float in_elevation;
  float3 in_position;
};

struct VSOutput {
  float4 pos : SV_Position;
  float point_size : SV_PointSize;
  float3 frag_color : COLOR0;
};

struct PSInput {
  float4 pos : SV_Position;
  float3 frag_color : COLOR0;
};

[shader("vertex")]
VSOutput vert_main(VSInput input) {
  VSOutput output;
  float4 worldPos = mul(camera.model, float4(input.in_position, 1.0));
  float4 viewPos = mul(camera.view, worldPos);
  output.pos = mul(camera.proj, viewPos);
  output.frag_color = float3(1.0, 1.0, 1.0);
  return output;
}

[shader("fragment")]
float4 frag_main(PSInput input) : SV_Target {
  return float4(input.frag_color, 1.0);
}
