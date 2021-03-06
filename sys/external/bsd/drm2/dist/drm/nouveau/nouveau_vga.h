/*	$NetBSD: nouveau_vga.h,v 1.3 2018/08/27 04:58:24 riastradh Exp $	*/

#ifndef __NOUVEAU_VGA_H__
#define __NOUVEAU_VGA_H__

struct nouveau_drm;
struct drm_device;

void nouveau_vga_init(struct nouveau_drm *);
void nouveau_vga_fini(struct nouveau_drm *);
void nouveau_vga_lastclose(struct drm_device *dev);

#endif
