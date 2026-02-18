"use client";

import { useEffect, useRef } from "react";
import * as THREE from "three";

const vertexShader = `
varying vec2 vUv;
uniform float uTime;
uniform float uEnableWaves;

void main() {
  vUv = uv;
  float time = uTime * 5.0;
  float waveFactor = uEnableWaves;

  vec3 transformed = position;
  transformed.x += sin(time + position.y) * 0.5 * waveFactor;
  transformed.y += cos(time + position.z) * 0.15 * waveFactor;
  transformed.z += sin(time + position.x) * waveFactor;

  gl_Position = projectionMatrix * modelViewMatrix * vec4(transformed, 1.0);
}
`;

const fragmentShader = `
varying vec2 vUv;
uniform float uTime;
uniform sampler2D uTexture;

void main() {
  float time = uTime;
  vec2 pos = vUv;

  float r = texture2D(uTexture, pos + cos(time + pos.x) * 0.01).r;
  float g = texture2D(uTexture, pos + tan(time * 0.5 + pos.x - time) * 0.01).g;
  float b = texture2D(uTexture, pos - cos(time + pos.y) * 0.01).b;
  float a = texture2D(uTexture, pos).a;
  gl_FragColor = vec4(r, g, b, a);
}
`;

const PX_RATIO = typeof window !== "undefined" ? window.devicePixelRatio : 1;

function mapRange(n: number, start: number, stop: number, start2: number, stop2: number): number {
  return ((n - start) / (stop - start)) * (stop2 - start2) + start2;
}

class AsciiFilter {
  renderer: THREE.WebGLRenderer;
  domElement: HTMLDivElement;
  pre: HTMLPreElement;
  canvas: HTMLCanvasElement;
  context: CanvasRenderingContext2D;
  width = 0;
  height = 0;
  cols = 0;
  rows = 0;
  center = { x: 0, y: 0 };
  mouse = { x: 0, y: 0 };
  deg = 0;
  invert: boolean;
  fontSize: number;
  fontFamily: string;
  charset: string;

  constructor(
    renderer: THREE.WebGLRenderer,
    options: { fontSize?: number; fontFamily?: string; charset?: string; invert?: boolean } = {}
  ) {
    this.renderer = renderer;
    this.domElement = document.createElement("div");
    this.domElement.style.position = "absolute";
    this.domElement.style.top = "0";
    this.domElement.style.left = "0";
    this.domElement.style.width = "100%";
    this.domElement.style.height = "100%";

    this.pre = document.createElement("pre");
    this.domElement.appendChild(this.pre);

    this.canvas = document.createElement("canvas");
    const ctx = this.canvas.getContext("2d");
    if (!ctx) {
      throw new Error("Failed to create 2D context for ASCII filter.");
    }
    this.context = ctx;
    // Keep canvas as an offscreen buffer only; don't render source text layer.
    this.canvas.style.display = "none";
    this.domElement.appendChild(this.canvas);

    this.invert = options.invert ?? true;
    this.fontSize = options.fontSize ?? 12;
    this.fontFamily = options.fontFamily ?? "'Courier New', monospace";
    this.charset =
      options.charset ??
      " .'`^\",:;Il!i~+_-?][}{1)(|/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

    this.context.imageSmoothingEnabled = false;
    this.onMouseMove = this.onMouseMove.bind(this);
    document.addEventListener("mousemove", this.onMouseMove);
  }

  setSize(width: number, height: number): void {
    this.width = width;
    this.height = height;
    this.renderer.setSize(width, height);
    this.reset();
    this.center = { x: width / 2, y: height / 2 };
    this.mouse = { x: this.center.x, y: this.center.y };
  }

  reset(): void {
    this.context.font = `${this.fontSize}px ${this.fontFamily}`;
    const charWidth = this.context.measureText("A").width;
    this.cols = Math.floor(this.width / (this.fontSize * (charWidth / this.fontSize)));
    this.rows = Math.floor(this.height / this.fontSize);

    this.canvas.width = this.cols;
    this.canvas.height = this.rows;

    this.pre.style.fontFamily = this.fontFamily;
    this.pre.style.fontSize = `${this.fontSize}px`;
    this.pre.style.margin = "0";
    this.pre.style.padding = "0";
    this.pre.style.lineHeight = "1em";
    this.pre.style.position = "absolute";
    this.pre.style.left = "0";
    this.pre.style.top = "0";
    this.pre.style.zIndex = "9";
    this.pre.style.mixBlendMode = "normal";
  }

  render(scene: THREE.Scene, camera: THREE.Camera): void {
    this.renderer.render(scene, camera);
    const w = this.canvas.width;
    const h = this.canvas.height;
    this.context.clearRect(0, 0, w, h);
    this.context.drawImage(this.renderer.domElement, 0, 0, w, h);
    this.asciify(this.context, w, h);
  }

  onMouseMove(e: MouseEvent): void {
    this.mouse = { x: e.clientX * PX_RATIO, y: e.clientY * PX_RATIO };
  }

  get dx(): number {
    return this.mouse.x - this.center.x;
  }

  get dy(): number {
    return this.mouse.y - this.center.y;
  }

  hue(): void {
    const deg = (Math.atan2(this.dy, this.dx) * 180) / Math.PI;
    this.deg += (deg - this.deg) * 0.075;
    this.domElement.style.filter = `hue-rotate(${this.deg.toFixed(1)}deg)`;
  }

  asciify(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    const imgData = ctx.getImageData(0, 0, w, h).data;
    let str = "";
    for (let y = 0; y < h; y += 1) {
      for (let x = 0; x < w; x += 1) {
        const i = x * 4 + y * 4 * w;
        const r = imgData[i];
        const g = imgData[i + 1];
        const b = imgData[i + 2];
        const a = imgData[i + 3];

        if (a === 0) {
          str += " ";
          continue;
        }

        const gray = (0.3 * r + 0.6 * g + 0.1 * b) / 255;
        let idx = Math.floor((1 - gray) * (this.charset.length - 1));
        if (this.invert) idx = this.charset.length - idx - 1;
        str += this.charset[idx];
      }
      str += "\n";
    }
    this.pre.textContent = str;
  }

  dispose(): void {
    document.removeEventListener("mousemove", this.onMouseMove);
  }
}

class CanvasText {
  canvas: HTMLCanvasElement;
  context: CanvasRenderingContext2D;
  text: string;
  fontSize: number;
  fontFamily: string;
  color: string;
  font: string;

  constructor(text: string, options: { fontSize?: number; fontFamily?: string; color?: string } = {}) {
    this.canvas = document.createElement("canvas");
    const ctx = this.canvas.getContext("2d");
    if (!ctx) {
      throw new Error("Failed to create 2D context for text canvas.");
    }
    this.context = ctx;
    this.text = text;
    this.fontSize = options.fontSize ?? 200;
    this.fontFamily = options.fontFamily ?? "Arial";
    this.color = options.color ?? "#fdf9f3";
    this.font = `600 ${this.fontSize}px ${this.fontFamily}`;
  }

  resize(): void {
    this.context.font = this.font;
    const metrics = this.context.measureText(this.text);
    const textWidth = Math.ceil(metrics.width) + 20;
    const textHeight = Math.ceil(metrics.actualBoundingBoxAscent + metrics.actualBoundingBoxDescent) + 20;
    this.canvas.width = textWidth;
    this.canvas.height = textHeight;
  }

  render(): void {
    this.context.clearRect(0, 0, this.canvas.width, this.canvas.height);
    this.context.fillStyle = this.color;
    this.context.font = this.font;
    const metrics = this.context.measureText(this.text);
    const yPos = 10 + metrics.actualBoundingBoxAscent;
    this.context.fillText(this.text, 10, yPos);
  }

  get width(): number {
    return this.canvas.width;
  }

  get height(): number {
    return this.canvas.height;
  }

  get texture(): HTMLCanvasElement {
    return this.canvas;
  }
}

class CanvasAscii {
  textString: string;
  asciiFontSize: number;
  textFontSize: number;
  textColor: string;
  planeBaseHeight: number;
  enableWaves: boolean;
  container: HTMLDivElement;
  width: number;
  height: number;
  camera: THREE.PerspectiveCamera;
  scene: THREE.Scene;
  mouse: { x: number; y: number };
  renderer?: THREE.WebGLRenderer;
  filter?: AsciiFilter;
  mesh?: THREE.Mesh<THREE.PlaneGeometry, THREE.ShaderMaterial>;
  geometry?: THREE.PlaneGeometry;
  material?: THREE.ShaderMaterial;
  texture?: THREE.CanvasTexture;
  textCanvas?: CanvasText;
  animationFrameId = 0;

  constructor(
    options: {
      text: string;
      asciiFontSize: number;
      textFontSize: number;
      textColor: string;
      planeBaseHeight: number;
      enableWaves: boolean;
    },
    containerElem: HTMLDivElement,
    width: number,
    height: number
  ) {
    this.textString = options.text;
    this.asciiFontSize = options.asciiFontSize;
    this.textFontSize = options.textFontSize;
    this.textColor = options.textColor;
    this.planeBaseHeight = options.planeBaseHeight;
    this.enableWaves = options.enableWaves;
    this.container = containerElem;
    this.width = width;
    this.height = height;

    this.camera = new THREE.PerspectiveCamera(45, this.width / this.height, 1, 1000);
    this.camera.position.z = 30;

    this.scene = new THREE.Scene();
    this.mouse = { x: this.width / 2, y: this.height / 2 };
    this.onMouseMove = this.onMouseMove.bind(this);
  }

  async init(): Promise<void> {
    if (typeof document !== "undefined" && document.fonts) {
      try {
        await document.fonts.load('600 200px "IBM Plex Mono"');
        await document.fonts.load('500 12px "IBM Plex Mono"');
        await document.fonts.ready;
      } catch {
        // Use fallback fonts if loading fails.
      }
    }
    this.setMesh();
    this.setRenderer();
  }

  setMesh(): void {
    this.textCanvas = new CanvasText(this.textString, {
      fontSize: this.textFontSize,
      fontFamily: "IBM Plex Mono",
      color: this.textColor
    });
    this.textCanvas.resize();
    this.textCanvas.render();

    this.texture = new THREE.CanvasTexture(this.textCanvas.texture);
    this.texture.minFilter = THREE.NearestFilter;

    const textAspect = this.textCanvas.width / this.textCanvas.height;
    const planeH = this.planeBaseHeight;
    const planeW = planeH * textAspect;

    this.geometry = new THREE.PlaneGeometry(planeW, planeH, 36, 36);
    this.material = new THREE.ShaderMaterial({
      vertexShader,
      fragmentShader,
      transparent: true,
      uniforms: {
        uTime: { value: 0 },
        uTexture: { value: this.texture },
        uEnableWaves: { value: this.enableWaves ? 1 : 0 }
      }
    });

    this.mesh = new THREE.Mesh(this.geometry, this.material);
    this.scene.add(this.mesh);
  }

  setRenderer(): void {
    this.renderer = new THREE.WebGLRenderer({ antialias: false, alpha: true });
    this.renderer.setPixelRatio(1);
    this.renderer.setClearColor(0x000000, 0);

    this.filter = new AsciiFilter(this.renderer, {
      fontFamily: "IBM Plex Mono",
      fontSize: this.asciiFontSize,
      invert: true
    });

    this.container.appendChild(this.filter.domElement);
    this.setSize(this.width, this.height);
    this.container.addEventListener("mousemove", this.onMouseMove);
    this.container.addEventListener("touchmove", this.onMouseMove as EventListener);
  }

  setSize(w: number, h: number): void {
    this.width = w;
    this.height = h;
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    if (this.filter) {
      this.filter.setSize(w, h);
    }
  }

  load(): void {
    this.animate();
  }

  onMouseMove(evt: MouseEvent | TouchEvent): void {
    const touchEvent = evt as TouchEvent;
    const e = touchEvent.touches && touchEvent.touches[0] ? touchEvent.touches[0] : (evt as MouseEvent);
    const bounds = this.container.getBoundingClientRect();
    this.mouse = { x: e.clientX - bounds.left, y: e.clientY - bounds.top };
  }

  animate(): void {
    const animateFrame = () => {
      this.animationFrameId = requestAnimationFrame(animateFrame);
      this.render();
    };
    animateFrame();
  }

  render(): void {
    const time = Date.now() * 0.001;
    if (!this.textCanvas || !this.texture || !this.mesh || !this.filter) return;

    this.textCanvas.render();
    this.texture.needsUpdate = true;
    this.mesh.material.uniforms.uTime.value = Math.sin(time);
    this.updateRotation();
    this.filter.render(this.scene, this.camera);
  }

  updateRotation(): void {
    if (!this.mesh) return;
    const x = mapRange(this.mouse.y, 0, this.height, 0.5, -0.5);
    const y = mapRange(this.mouse.x, 0, this.width, -0.5, 0.5);
    this.mesh.rotation.x += (x - this.mesh.rotation.x) * 0.05;
    this.mesh.rotation.y += (y - this.mesh.rotation.y) * 0.05;
  }

  clear(): void {
    this.scene.traverse((obj) => {
      const mesh = obj as THREE.Mesh;
      if (!mesh.isMesh) return;
      if (mesh.material) {
        const material = mesh.material as THREE.Material | THREE.Material[];
        if (Array.isArray(material)) {
          material.forEach((mat) => mat.dispose());
        } else {
          material.dispose();
        }
      }
      if (mesh.geometry) {
        mesh.geometry.dispose();
      }
    });
    this.scene.clear();
  }

  dispose(): void {
    cancelAnimationFrame(this.animationFrameId);
    if (this.filter?.domElement.parentNode) {
      this.container.removeChild(this.filter.domElement);
    }
    this.filter?.dispose();
    this.container.removeEventListener("mousemove", this.onMouseMove);
    this.container.removeEventListener("touchmove", this.onMouseMove as EventListener);
    this.clear();
    this.renderer?.dispose();
  }
}

type ASCIITextProps = {
  text?: string;
  asciiFontSize?: number;
  textFontSize?: number;
  textColor?: string;
  planeBaseHeight?: number;
  enableWaves?: boolean;
};

export default function ASCIIText({
  text = "Kiwi Analytics",
  asciiFontSize = 1,
  textFontSize = 200,
  textColor = "#fdf9f3",
  planeBaseHeight = 8,
  enableWaves = true
}: ASCIITextProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const asciiRef = useRef<CanvasAscii | null>(null);

  useEffect(() => {
    if (!containerRef.current) return;

    let cancelled = false;
    let observer: IntersectionObserver | null = null;
    let resizeObserver: ResizeObserver | null = null;

    const createAndInit = async (container: HTMLDivElement, w: number, h: number): Promise<CanvasAscii> => {
      const instance = new CanvasAscii(
        { text, asciiFontSize, textFontSize, textColor, planeBaseHeight, enableWaves },
        container,
        w,
        h
      );
      await instance.init();
      return instance;
    };

    const setup = async () => {
      if (!containerRef.current) return;
      const { width, height } = containerRef.current.getBoundingClientRect();

      if (width === 0 || height === 0) {
        observer = new IntersectionObserver(async ([entry]) => {
          if (cancelled || !entry.isIntersecting) return;
          const w = entry.boundingClientRect.width;
          const h = entry.boundingClientRect.height;
          if (w <= 0 || h <= 0 || !containerRef.current) return;
          observer?.disconnect();
          observer = null;

          asciiRef.current = await createAndInit(containerRef.current, w, h);
          if (!cancelled) {
            asciiRef.current.load();
          }
        });
        observer.observe(containerRef.current);
        return;
      }

      asciiRef.current = await createAndInit(containerRef.current, width, height);
      if (cancelled || !asciiRef.current) return;

      asciiRef.current.load();
      resizeObserver = new ResizeObserver((entries) => {
        if (!asciiRef.current || !entries[0]) return;
        const w = entries[0].contentRect.width;
        const h = entries[0].contentRect.height;
        if (w > 0 && h > 0) {
          asciiRef.current.setSize(w, h);
        }
      });
      resizeObserver.observe(containerRef.current);
    };

    void setup();

    return () => {
      cancelled = true;
      observer?.disconnect();
      resizeObserver?.disconnect();
      asciiRef.current?.dispose();
      asciiRef.current = null;
    };
  }, [text, asciiFontSize, textFontSize, textColor, planeBaseHeight, enableWaves]);

  return (
    <div ref={containerRef} className="ascii-text-container absolute inset-0 h-full w-full">
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@500;600&display=swap');
        .ascii-text-container canvas {
          position: absolute;
          left: 0;
          top: 0;
          width: 100%;
          height: 100%;
          image-rendering: pixelated;
        }
        .ascii-text-container pre {
          margin: 0;
          user-select: none;
          padding: 0;
          line-height: 1em;
          text-align: left;
          position: absolute;
          left: 0;
          top: 0;
          z-index: 9;
          color: #dcf2e4;
          font-weight: 600;
          text-shadow: 0 0 8px rgba(139, 211, 173, 0.75), 0 0 2px rgba(240, 249, 244, 0.9);
          mix-blend-mode: normal;
        }
      `}</style>
    </div>
  );
}
