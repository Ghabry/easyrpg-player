#!/usr/bin/env ruby
# encoding: utf-8

# Compiles the shader code for use with SPIRV (Vulkan) and MSL (Metal/Apple)

# DXIL (DirectX Shader) are created during compile-time on Windows
# dxc -T ps_6_0 -E main -Vn sprite_frag_dxil -Fh sprite_frag_dxil.h sprite.frag.hlsl
# dxc -T vs_6_0 -E main -Vn sprite_vert_dxil -Fh sprite_vert_dxil.h sprite.frag.hlsl

Dir.chdir(__dir__)

require "open3"

def which(cmd)
  exts = ENV['PATHEXT'] ? ENV['PATHEXT'].split(';') : ['']
  ENV['PATH'].split(File::PATH_SEPARATOR).each do |path|
    exts.each do |ext|
      exe = File.join(path, "#{cmd}#{ext}")
      return exe if File.executable?(exe) && !File.directory?(exe)
    end
  end
  nil
end

glslc = which("glslc")

if glslc.nil?
  abort "Error: glslc not found in PATH"
end

spirvcross = which("spirv-cross")


OUTPUT_FOLDER='../../src/generated'

def build_shader_spirv(f)
  stage = f.include?(".frag") ? "fragment" : "vertex"

  stdout, stderr, status = Open3.capture3(
    "glslc",
    "-mfmt=c",
    "-fshader-stage=#{stage}",
    f + ".hlsl",
    "-o", "-"
  )

  unless status.success?
    warn "SPIRV: Shader compilation of #{f} failed!"
    warn stderr
    exit 1
  end

  stdout.strip
end

def build_shader_msl(f)
  stdout, stderr, status = Open3.capture3(
    "spirv-cross",
    "--msl",
    f + ".spv"
  )

  unless status.success?
    warn "MSL: Shader compilation of #{f} failed!"
    warn stderr
    exit 1
  end

  stdout.lines.map(&:strip).reject(&:empty?).join("\n")
end

def write(f, sym, vert_spirv, frag_spirv, vert_msl, frag_msl)
  f.write <<EOS
/* !!!! GENERATED FILE - DO NOT EDIT !!!!
 * --------------------------------------
 */
#ifndef EP_SHADER_#{sym.upcase}_H
#define EP_SHADER_#{sym.upcase}_H

#include <cstdint>

const uint32_t shader_#{sym}_vert_spirv[] =
#{vert_spirv};

const uint32_t shader_#{sym}_frag_spirv[] =
#{frag_spirv};

#ifdef __APPLE__
const char* const shader_#{sym}_vert_metal = R"(
#{vert_msl}
)";

const char* const shader_#{sym}_frag_metal = R"(
#{frag_msl}
)";
#else
const char* const shader_#{sym}_vert_metal = "";

const char* const shader_#{sym}_frag_metal = "";
#endif

#ifndef SHADER_#{sym.upcase}_DXIL
const uint8_t shader_sprite_vert_dxil[] = { 0x0 };
const uint8_t shader_sprite_frag_dxil[] = { 0x0 };
#endif

#endif
EOS
end

sprite_vert_spirv = build_shader_spirv("sprite.vert")
sprite_frag_spirv = build_shader_spirv("sprite.frag")

sprite_vert_msl = ""
sprite_frag_msl = ""

if not spirvcross.nil?
  sprite_vert_msl = build_shader_msl("sprite.vert")
  sprite_frag_msl = build_shader_msl("sprite.frag")
end

write(File.new("#{OUTPUT_FOLDER}/shader_sprite.h", "w"), "sprite",
  sprite_vert_spirv, sprite_frag_spirv, sprite_vert_msl, sprite_frag_msl)

if spirvcross.nil?
  puts "spirv-cross not found! The MSL shader for macOS was not built!"
end
