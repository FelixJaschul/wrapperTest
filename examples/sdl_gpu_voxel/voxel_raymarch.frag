#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

layout(std140, set = 3, binding = 0) uniform FragmentUniforms {
    vec4 cam_pos;
    vec4 cam_right;
    vec4 cam_up;
    vec4 cam_forward;
    vec4 screen_time; // x=width, y=height, z=time_sec, w=tan_half_fov
    vec4 render_cfg;  // x=aspect, y=grid_size, z=max_dist, w=max_steps
} ubo;

float stable_hash(vec3 p)
{
    return fract(sin(dot(p, vec3(12.9898, 78.233, 39.425))) * 43758.5453);
}

bool voxel_occupied(ivec3 c)
{
    // Minimal default scene: sparse noisy voxel field.
    const vec3 extent = vec3(1400.0, 8.0, 1400.0);
    if (abs(float(c.x)) > extent.x || abs(float(c.y)) > extent.y || abs(float(c.z)) > extent.z) return false;

    return stable_hash(vec3(c) * 0.12) > 0.68;
}

vec3 voxel_color(ivec3 c)
{
    return mix(vec3(0.0, 0.0, 0.0), vec3(0.95, 0.0, 0.76), stable_hash(vec3(c)));
}

bool raycast_voxels(vec3 ro, vec3 rd, float grid_size, float max_dist, int max_steps, out vec3 hit_pos, out vec3 hit_normal, out ivec3 hit_cell)
{
    ivec3 cell = ivec3(floor(ro / grid_size));

    ivec3 step_dir = ivec3(
        rd.x > 0.0 ? 1 : (rd.x < 0.0 ? -1 : 0),
        rd.y > 0.0 ? 1 : (rd.y < 0.0 ? -1 : 0),
        rd.z > 0.0 ? 1 : (rd.z < 0.0 ? -1 : 0)
    );

    vec3 delta = vec3(1e30);
    vec3 t_max = vec3(1e30);

    vec3 cell_min = vec3(cell) * grid_size;
    vec3 cell_max = cell_min + vec3(grid_size);

    if (abs(rd.x) > 1e-6) {
        delta.x = abs(grid_size / rd.x);
        t_max.x = ((rd.x > 0.0 ? cell_max.x : cell_min.x) - ro.x) / rd.x;
    }
    if (abs(rd.y) > 1e-6) {
        delta.y = abs(grid_size / rd.y);
        t_max.y = ((rd.y > 0.0 ? cell_max.y : cell_min.y) - ro.y) / rd.y;
    }
    if (abs(rd.z) > 1e-6) {
        delta.z = abs(grid_size / rd.z);
        t_max.z = ((rd.z > 0.0 ? cell_max.z : cell_min.z) - ro.z) / rd.z;
    }

    float t = 0.0;
    int last_axis = -1;

    for (int i = 0; i < max_steps; ++i) {
        if (voxel_occupied(cell)) {
            hit_pos = ro + rd * max(t, 0.0);
            hit_cell = cell;

            if (last_axis == 0) hit_normal = vec3(-float(step_dir.x), 0.0, 0.0);
            else if (last_axis == 1) hit_normal = vec3(0.0, -float(step_dir.y), 0.0);
            else if (last_axis == 2) hit_normal = vec3(0.0, 0.0, -float(step_dir.z));
            else hit_normal = -sign(rd);

            return true;
        }

        if (t > max_dist) break;

        int axis;
        if (t_max.x < t_max.y) axis = (t_max.x < t_max.z) ? 0 : 2;
        else axis = (t_max.y < t_max.z) ? 1 : 2;

        if (axis == 0) {
            t = t_max.x;
            t_max.x += delta.x;
            cell.x += step_dir.x;
        } else if (axis == 1) {
            t = t_max.y;
            t_max.y += delta.y;
            cell.y += step_dir.y;
        } else {
            t = t_max.z;
            t_max.z += delta.z;
            cell.z += step_dir.z;
        }

        last_axis = axis;
    }

    return false;
}

void main()
{
    vec2 ndc = vec2(v_uv.x * 2.0 - 1.0, v_uv.y * 2.0 - 1.0);

    vec3 ro = ubo.cam_pos.xyz;
    vec3 rd = normalize(
        ubo.cam_forward.xyz +
        ndc.x * ubo.render_cfg.x * ubo.screen_time.w * ubo.cam_right.xyz +
        ndc.y * ubo.screen_time.w * ubo.cam_up.xyz
    );

    vec3 sky = vec3(0.0, 0.0, 0.0);
    vec3 color = mix(sky, sky, clamp(rd.y * 0.5 + 0.5, 0.0, 1.0));

    vec3 hit_pos;
    vec3 hit_normal;
    ivec3 hit_cell;

    bool hit = raycast_voxels(
        ro, rd,
        ubo.render_cfg.y,
        ubo.render_cfg.z,
        int(ubo.render_cfg.w),
        hit_pos,
        hit_normal,
        hit_cell
    );

    if (hit) {
        vec3 local = fract(hit_pos / ubo.render_cfg.y);
        float edge = min(
            min(min(local.x, local.y), local.z),
            min(min(1.0 - local.x, 1.0 - local.y), 1.0 - local.z)
        );

        color = voxel_color(hit_cell) * max(dot(hit_normal, normalize(vec3(0.6, 1.0, 0.3))), 0.12);
        color += smoothstep(0.08, 0.0, edge) * 0.10;
        color = mix(color, sky, clamp(length(hit_pos - ro) / ubo.render_cfg.z, 0.0, 1.0) * 0.55);
    }

    out_color = vec4(color, 1.0);
}
