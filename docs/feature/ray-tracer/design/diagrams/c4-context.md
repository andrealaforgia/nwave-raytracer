# C4 Context Diagram: nwave-raytracer

## Diagram

```mermaid
graph TB
    user["<b>Developer / Artist</b><br/>Defines 3D scenes,<br/>renders images,<br/>iterates on designs"]

    nwave["<b>nwave-raytracer</b><br/><i>C++ CLI Application</i><br/>Renders photorealistic 3D<br/>scenes from YAML scene files"]

    editor["<b>Text Editor</b><br/><i>External System</i><br/>VS Code, Vim, etc.<br/>Used to author YAML scene files"]

    viewer["<b>Image Viewer</b><br/><i>External System</i><br/>GIMP, Preview, IrfanView<br/>Used to view rendered images"]

    fs["<b>File System</b><br/><i>External System</i><br/>Stores scene files (.yaml)<br/>and rendered images (.ppm/.png)"]

    user -->|"Edits scene files"| editor
    user -->|"Runs CLI commands:<br/>nwave validate / render"| nwave
    user -->|"Views rendered images"| viewer
    editor -->|"Reads/writes scene files"| fs
    nwave -->|"Reads scene YAML<br/>Writes image files"| fs
    viewer -->|"Reads image files"| fs

    classDef person fill:#08427B,stroke:#073B6F,color:#fff
    classDef system fill:#1168BD,stroke:#0E5AA0,color:#fff
    classDef external fill:#999999,stroke:#6B6B6B,color:#fff

    class user person
    class nwave system
    class editor,viewer,fs external
```

## Description

The system context shows nwave-raytracer as a single CLI application that:

1. **Receives** YAML scene files authored by the user in a text editor
2. **Produces** rendered image files (PPM or PNG) on the file system
3. **Is viewed** by the user through standard image viewing tools

There are no network dependencies, databases, or external services. The system is entirely self-contained and operates on local files.

## Actors

| Actor | Description |
|---|---|
| Developer / Artist | The primary user -- defines scenes, runs renders, iterates on designs. May be a CG student (Elena), hobbyist artist (David), CS instructor (Prof. Tanaka), or technical artist (Sofia). |

## External Systems

| System | Interaction |
|---|---|
| Text Editor | User creates and edits YAML scene files. No integration with nwave beyond shared file system. |
| Image Viewer | User opens rendered images. No integration with nwave beyond shared file system. |
| File System | The shared boundary: nwave reads scene files and writes image files. |
