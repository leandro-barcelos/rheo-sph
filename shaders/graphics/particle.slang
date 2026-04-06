struct VSInput {
  float3 in_position;
  float3 in_velocity;
  float in_density;
};

struct VSOutput {
  float4 pos : SV_Position;
  float point_size : SV_PointSize;
  float3 frag_color : COLOR0;
};

struct PSInput {
  float4 pos : SV_Position;
  float3 frag_color : COLOR0;
  float2 point_coord : SV_PointCoord;
};

[shader("vertex")]
VSOutput vert_main(VSInput input) {
  VSOutput output;
  output.pos = float4(input.in_position, 1.0);
  output.point_size = 4.0;
  output.frag_color = float3(0.0, 0.0, 1.0);
  return output;
}

[shader("fragment")]
float4 frag_main(PSInput input) : SV_Target {
  float2 coord = input.point_coord - float2(0.5);
  float alpha = max(0.0, 0.5 - length(coord));
  return float4(input.frag_color, alpha);
}
