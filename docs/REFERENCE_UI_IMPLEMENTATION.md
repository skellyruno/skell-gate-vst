# Skell-Glitch — reference UI implementation brief

**Source of truth:** User-provided Skell-Glitch Ivory screenshot, 1448×1086 (4:3).

**Instruction to the implementer:** Reconstruct the *interactive JUCE editor*, not a static screenshot. Keep all existing audio parameters and controls. Aim for the reference's champagne/brushed-ivory chassis, inset metallic waveform, bright lime selected buttons, dimensional circular dials, small embossed preset controls, frequency rail, restrained brown typography, and highlighted circular skull. Keep control positions, hierarchy, and labels faithful to the image.

**Shared identity:** Pin skellyruno/skellcore-vst-template at `d02d50849123f28bbc2e32cded0e78aced15afb3`. Link SkellCore::UI, call SkellCore's official `drawSkullKeyMark` geometry within the round skull badge, use `StudioLookAndFeel`/StudioSkinStore palettes, and implement skull-click skin cycling. Ivory is the default new-user skin; its neon green is a brighter reference-specific treatment. Other Studio skins are shared across participating plugins.

**Plan/checklist:**
1. Match geometry in a 960×720 design canvas; scalable presets 480×360, 720×540 (default), 960×720, and 1200×900.
2. Draw a one-time cached brushed metal chassis and bevel details.
3. Style every real button, dial, combo, panel, gate step and band handle in JUCE; preserve parameter attachments.
4. Render the real editor at all sizes and compare layout/geometry with the reference. Test skull theme switching and host-visible controls.
5. Run dependency-free DSP checks and full Windows VST3 build and smoke tests; correct warnings/errors.

Visual fidelity is bounded by differences in operating-system font rasterization and procedural brushed texture. The screenshot is a reference, not a bitmap pasted under the controls.
