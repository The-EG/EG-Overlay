// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Linear algebra (matrix) related math stuff

/// A 4x4 matrix of 32-bit floats
/// arranged in column major format
///
/// i1j1  i1j2  i1j3  i1j4
///
/// i2j1  i2j2  i2j3  i2j4
///
/// i3j1  i3j2  i3j3  i3j4
///
/// i4j1  i4j2  i4j3  i4j4
#[repr(C)]
#[derive(Default)]
pub struct Mat4F {
    i1j1: f32,
    i2j1: f32,
    i3j1: f32,
    i4j1: f32,

    i1j2: f32,
    i2j2: f32,
    i3j2: f32,
    i4j2: f32,

    i1j3: f32,
    i2j3: f32,
    i3j3: f32,
    i4j3: f32,

    i1j4: f32,
    i2j4: f32,
    i3j4: f32,
    i4j4: f32,
}

// helper functions for calculating the inverse of a Mat4F below
fn mat2f_determinate(a: f32, b: f32, c: f32, d: f32) -> f32 {
    (a * d) - (b * c)
}

fn mat3f_determinate(a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, g: f32, h: f32, i: f32) -> f32 {
    (a * mat2f_determinate(e, f, h, i)) -
    (b * mat2f_determinate(d, f, g, i)) +
    (c * mat2f_determinate(d, e, g, h))
}

impl Mat4F {
    pub fn ortho(left: f32, right: f32, top: f32, bottom: f32, near: f32, far: f32) -> Mat4F {
        Mat4F {
            i1j1: 2.0 / (right - left),
            i2j1: 0.0,
            i3j1: 0.0,
            i4j1: (right + left) / (left - right),

            i1j2: 0.0,
            i2j2: 2.0 / (top - bottom),
            i3j2: 0.0,
            i4j2: (top + bottom) / (bottom - top),

            i1j3: 0.0,
            i2j3: 0.0,
            i3j3: 1.0 / (far - near),
            i4j3: near / (near - far),

            i1j4: 0.0,
            i2j4: 0.0,
            i3j4: 0.0,
            i4j4: 1.0,
        }
    }
    
    pub fn perspective_lh(fovy: f32, aspect: f32, near: f32, far: f32) -> Mat4F {
        let mut m = Mat4F::default();

        let yscale = 1.0f32 / (fovy/2.0f32).tan();
        let xscale = yscale / aspect;

        m.i1j1 = xscale;
        m.i2j1 = 0.0;
        m.i3j1 = 0.0;
        m.i4j1 = 0.0;

        m.i1j2 = 0.0;
        m.i2j2 = yscale;
        m.i3j2 = 0.0;
        m.i4j2 = 0.0;

        m.i1j3 = 0.0;
        m.i2j3 = 0.0;
        m.i3j3 = far / (far - near);
        m.i4j3 = (-near * far) / (far - near);

        m.i1j4 = 0.0;
        m.i2j4 = 0.0;
        m.i3j4 = 1.0;
        m.i4j4 = 0.0;

        return m;
    }

    pub fn camera_facing(camera: &Vec3F, forward: &Vec3F, up: &Vec3F) -> Mat4F {
        let n_up = up.normalize();

        let side = n_up.crossproduct(&forward);
        let n_side = side.normalize();

        let real_up = forward.crossproduct(&n_side);
        let n_real_up = real_up.normalize();

        let a = n_side.dot(&camera);
        let b = n_real_up.dot(&camera);
        let c = forward.dot(&camera);

        Mat4F {
            i1j1: n_side.x,
            i2j1: n_side.y,
            i3j1: n_side.z,
            i4j1: -a,

            i1j2: n_real_up.x,
            i2j2: n_real_up.y,
            i3j2: n_real_up.z,
            i4j2: -b,

            i1j3: forward.x,
            i2j3: forward.y,
            i3j3: forward.z,
            i4j3: -c,

            i1j4: 0.0,
            i2j4: 0.0,
            i3j4: 0.0,
            i4j4: 1.0,
        }
    }

    pub fn identity() -> Mat4F {
        Mat4F {
            i1j1: 1.0, i1j2: 0.0, i1j3: 0.0, i1j4: 0.0,
            i2j1: 0.0, i2j2: 1.0, i2j3: 0.0, i2j4: 0.0,
            i3j1: 0.0, i3j2: 0.0, i3j3: 1.0, i3j4: 0.0,
            i4j1: 0.0, i4j2: 0.0, i4j3: 0.0, i4j4: 1.0,
        }
    }

    pub fn translate(x: f32, y: f32, z: f32) -> Mat4F {
        let mut m = Self::identity();

        m.i4j1 = x;
        m.i4j2 = y;
        m.i4j3 = z;

        m
    }

    pub fn rotatex(radians: f32) -> Mat4F {
        let mut m = Self::identity();

        m.i2j2 = radians.cos();
        m.i3j2 = -radians.sin();
        m.i2j3 = radians.sin();
        m.i3j3 = radians.cos();

        m
    }

    pub fn rotatey(radians: f32) -> Mat4F {
        let mut m = Self::identity();

        m.i1j1 = radians.cos();
        m.i3j1 = radians.sin();
        m.i1j3 = -radians.sin();
        m.i3j3 = radians.cos();

        m
    }

    pub fn rotatez(radians: f32) -> Mat4F {
        let mut m = Self::identity();

        m.i1j1 = radians.cos();
        m.i2j1 = - radians.sin();
        m.i1j2 = radians.sin();
        m.i2j2 = radians.cos();

        m
    }

    pub fn minors(&self) -> Mat4F {
        Mat4F {
            i1j1: mat3f_determinate(self.i2j2, self.i2j3, self.i2j4,
                                    self.i3j2, self.i3j3, self.i3j4,
                                    self.i4j2, self.i4j3, self.i4j4),

            i1j2: mat3f_determinate(self.i2j1, self.i2j3, self.i2j4,
                                    self.i3j1, self.i3j3, self.i3j4,
                                    self.i4j1, self.i4j3, self.i4j4),

            i1j3: mat3f_determinate(self.i2j1, self.i2j2, self.i2j4,
                                    self.i3j1, self.i3j2, self.i3j4,
                                    self.i4j1, self.i4j2, self.i4j4),

            i1j4: mat3f_determinate(self.i2j1, self.i2j2, self.i2j3,
                                    self.i3j1, self.i3j2, self.i3j3,
                                    self.i4j1, self.i4j2, self.i4j3),

            i2j1: mat3f_determinate(self.i1j2, self.i1j3, self.i1j4,
                                    self.i3j2, self.i3j3, self.i3j4,
                                    self.i4j2, self.i4j3, self.i4j4),

            i2j2: mat3f_determinate(self.i1j1, self.i1j3, self.i1j4,
                                    self.i3j1, self.i3j3, self.i3j4,
                                    self.i4j1, self.i4j3, self.i4j4),

            i2j3: mat3f_determinate(self.i1j1, self.i1j2, self.i1j4,
                                    self.i3j1, self.i3j2, self.i3j4,
                                    self.i4j1, self.i4j2, self.i4j4),

            i2j4: mat3f_determinate(self.i1j1, self.i1j2, self.i1j3,
                                    self.i3j1, self.i3j2, self.i3j3,
                                    self.i4j1, self.i4j2, self.i4j3),

            i3j1: mat3f_determinate(self.i1j2, self.i1j3, self.i1j4,
                                    self.i2j2, self.i2j3, self.i2j4,
                                    self.i4j2, self.i4j3, self.i4j4),

            i3j2: mat3f_determinate(self.i1j1, self.i1j3, self.i1j4,
                                    self.i2j1, self.i2j3, self.i2j4,
                                    self.i4j1, self.i4j3, self.i4j4),

            i3j3: mat3f_determinate(self.i1j1, self.i1j2, self.i1j4,
                                    self.i2j1, self.i2j2, self.i2j4,
                                    self.i4j1, self.i4j2, self.i4j4),

            i3j4: mat3f_determinate(self.i1j1, self.i1j2, self.i1j3,
                                    self.i2j1, self.i2j2, self.i2j3,
                                    self.i4j1, self.i4j2, self.i4j3),

            i4j1: mat3f_determinate(self.i1j2, self.i1j3, self.i1j4,
                                    self.i2j2, self.i2j3, self.i2j4,
                                    self.i3j2, self.i3j3, self.i3j4),

            i4j2: mat3f_determinate(self.i1j1, self.i1j3, self.i1j4,
                                    self.i2j1, self.i2j3, self.i2j4,
                                    self.i3j1, self.i3j3, self.i3j4),

            i4j3: mat3f_determinate(self.i1j1, self.i1j2, self.i1j4,
                                    self.i2j1, self.i2j2, self.i2j4,
                                    self.i3j1, self.i3j2, self.i3j4),

            i4j4: mat3f_determinate(self.i1j1, self.i1j2, self.i1j3,
                                    self.i2j1, self.i2j2, self.i2j3,
                                    self.i3j1, self.i3j2, self.i3j3),
        }
    }

    pub fn cofactors(&self) -> Mat4F {
        Mat4F {
            i1j1: self.i1j1,
            i1j2: self.i1j2 * -1.0,
            i1j3: self.i1j3,
            i1j4: self.i1j4 * -1.0,

            i2j1: self.i2j1 * -1.0,
            i2j2: self.i2j2,
            i2j3: self.i2j3 * -1.0,
            i2j4: self.i2j4,

            i3j1: self.i3j1,
            i3j2: self.i3j2 * -1.0,
            i3j3: self.i3j3,
            i3j4: self.i3j4 * -1.0,

            i4j1: self.i4j1 * -1.0,
            i4j2: self.i4j2,
            i4j3: self.i4j3 * -1.0,
            i4j4: self.i4j4,
        }
    }

    pub fn adjugate(&self) -> Mat4F {
        Mat4F {
            i1j1: self.i1j1,
            i1j2: self.i2j1,
            i1j3: self.i3j1,
            i1j4: self.i4j1,

            i2j1: self.i1j2,
            i2j2: self.i2j2,
            i2j3: self.i3j2,
            i2j4: self.i4j2,

            i3j1: self.i1j3,
            i3j2: self.i2j3,
            i3j3: self.i3j3,
            i3j4: self.i4j3,

            i4j1: self.i1j4,
            i4j2: self.i2j4,
            i4j3: self.i3j4,
            i4j4: self.i4j4,
        }
    }

    pub fn determinate(&self) -> f32 {
        (self.i1j1 * mat3f_determinate(self.i2j2, self.i2j3, self.i2j4,
                                       self.i3j2, self.i3j3, self.i3j4,
                                       self.i4j2, self.i4j3, self.i4j4)) -
        (self.i1j2 * mat3f_determinate(self.i2j1, self.i2j3, self.i2j4,
                                       self.i3j1, self.i3j3, self.i3j4,
                                       self.i4j1, self.i4j3, self.i4j4)) +
        (self.i1j3 * mat3f_determinate(self.i2j1, self.i2j2, self.i2j4,
                                       self.i3j1, self.i3j2, self.i3j4,
                                       self.i4j1, self.i4j2, self.i4j4)) -
        (self.i1j4 * mat3f_determinate(self.i2j1, self.i2j2, self.i2j3,
                                       self.i3j1, self.i3j2, self.i3j3,
                                       self.i4j1, self.i4j2, self.i4j3))
    }

    pub fn inverse(&self) -> Mat4F {
        let minors = self.minors();
        let cofactors = minors.cofactors();
        let adjugate = cofactors.adjugate();

        let oneoverd = 1.0 / self.determinate();

        Mat4F {
            i1j1: adjugate.i1j1 * oneoverd,
            i1j2: adjugate.i1j2 * oneoverd,
            i1j3: adjugate.i1j3 * oneoverd,
            i1j4: adjugate.i1j4 * oneoverd,

            i2j1: adjugate.i2j1 * oneoverd,
            i2j2: adjugate.i2j2 * oneoverd,
            i2j3: adjugate.i2j3 * oneoverd,
            i2j4: adjugate.i2j4 * oneoverd,

            i3j1: adjugate.i3j1 * oneoverd,
            i3j2: adjugate.i3j2 * oneoverd,
            i3j3: adjugate.i3j3 * oneoverd,
            i3j4: adjugate.i3j4 * oneoverd,
            
            i4j1: adjugate.i4j1 * oneoverd,
            i4j2: adjugate.i4j2 * oneoverd,
            i4j3: adjugate.i4j3 * oneoverd,
            i4j4: adjugate.i4j4 * oneoverd,
        }
    }
}

impl std::ops::Mul<Mat4F> for Mat4F {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self {
        Self {
            i1j1: self.i1j1 * rhs.i1j1 + self.i1j2 * rhs.i2j1 + self.i1j3 * rhs.i3j1 + self.i1j4 * rhs.i4j1,
            i2j1: self.i2j1 * rhs.i1j1 + self.i2j2 * rhs.i2j1 + self.i2j3 * rhs.i3j1 + self.i2j4 * rhs.i4j1,
            i3j1: self.i3j1 * rhs.i1j1 + self.i3j2 * rhs.i2j1 + self.i3j3 * rhs.i3j1 + self.i3j4 * rhs.i4j1,
            i4j1: self.i4j1 * rhs.i1j1 + self.i4j2 * rhs.i2j1 + self.i4j3 * rhs.i3j1 + self.i4j4 * rhs.i4j1,

            i1j2: self.i1j1 * rhs.i1j2 + self.i1j2 * rhs.i2j2 + self.i1j3 * rhs.i3j2 + self.i1j4 * rhs.i4j2,
            i2j2: self.i2j1 * rhs.i1j2 + self.i2j2 * rhs.i2j2 + self.i2j3 * rhs.i3j2 + self.i2j4 * rhs.i4j2,
            i3j2: self.i3j1 * rhs.i1j2 + self.i3j2 * rhs.i2j2 + self.i3j3 * rhs.i3j2 + self.i3j4 * rhs.i4j2,
            i4j2: self.i4j1 * rhs.i1j2 + self.i4j2 * rhs.i2j2 + self.i4j3 * rhs.i3j2 + self.i4j4 * rhs.i4j2,

            i1j3: self.i1j1 * rhs.i1j3 + self.i1j2 * rhs.i2j3 + self.i1j3 * rhs.i3j3 + self.i1j4 * rhs.i4j3,
            i2j3: self.i2j1 * rhs.i1j3 + self.i2j2 * rhs.i2j3 + self.i2j3 * rhs.i3j3 + self.i2j4 * rhs.i4j3,
            i3j3: self.i3j1 * rhs.i1j3 + self.i3j2 * rhs.i2j3 + self.i3j3 * rhs.i3j3 + self.i3j4 * rhs.i4j3,
            i4j3: self.i4j1 * rhs.i1j3 + self.i4j2 * rhs.i2j3 + self.i4j3 * rhs.i3j3 + self.i4j4 * rhs.i4j3,

            i1j4: self.i1j1 * rhs.i1j4 + self.i1j2 * rhs.i2j4 + self.i1j3 * rhs.i3j4 + self.i1j4 * rhs.i4j4,
            i2j4: self.i2j1 * rhs.i1j4 + self.i2j2 * rhs.i2j4 + self.i2j3 * rhs.i3j4 + self.i2j4 * rhs.i4j4,
            i3j4: self.i3j1 * rhs.i1j4 + self.i3j2 * rhs.i2j4 + self.i3j3 * rhs.i3j4 + self.i3j4 * rhs.i4j4,
            i4j4: self.i4j1 * rhs.i1j4 + self.i4j2 * rhs.i2j4 + self.i4j3 * rhs.i3j4 + self.i4j4 * rhs.i4j4,
        }
    }
}

impl std::ops::Mul<Vec4F> for Mat4F {
    type Output = Vec4F;

    fn mul(self, rhs: Vec4F) -> Vec4F {
        Vec4F {
            x: (rhs.x * self.i1j1) + (rhs.y * self.i2j1) + (rhs.z * self.i3j1) + (rhs.w * self.i4j1),
            y: (rhs.x * self.i1j2) + (rhs.y * self.i2j2) + (rhs.z * self.i3j2) + (rhs.w * self.i4j2),
            z: (rhs.x * self.i1j3) + (rhs.y * self.i2j3) + (rhs.z * self.i3j3) + (rhs.w * self.i4j3),
            w: (rhs.x * self.i1j4) + (rhs.y * self.i2j4) + (rhs.z * self.i3j4) + (rhs.w * self.i4j4),
        }
    }
}

#[repr(C)]
#[derive(Default,Clone,Copy)]
pub struct Vec3F {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl Vec3F {
    pub fn length(&self) -> f32 {
        ((self.x * self.x) + (self.y * self.y) + (self.z * self.z)).sqrt()
    }

    pub fn normalize(&self) -> Vec3F {
        let len = ((self.x * self.x) + (self.y * self.y) + (self.z * self.z)).sqrt();

        Vec3F {
            x: self.x / len,
            y: self.y / len,
            z: self.z / len,
        }
    }

    pub fn crossproduct(&self, other: &Vec3F) -> Vec3F {
        Vec3F {
            x: self.y * other.z - self.z * other.y,
            y: self.z * other.x - self.x * other.z,
            z: self.x * other.y - self.y * other.x,
        }
    }

    pub fn dot(&self, other: &Vec3F) -> f32 {
        (self.x * other.x) +
        (self.y * other.y) +
        (self.z * other.z)
    }

    pub fn mulf(&self, rhs: f32) -> Self {
        Self {
            x: self.x * rhs,
            y: self.y * rhs,
            z: self.z * rhs,
        }
    }
}

impl std::ops::Sub for Vec3F {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x - rhs.x,
            y: self.y - rhs.y,
            z: self.z - rhs.z,
        }
    }
}

impl std::ops::Add for Vec3F {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
            z: self.z + rhs.z,
        }
    }
}


#[repr(C)]
#[derive(Default,Clone,Copy)]
pub struct Vec4F {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}
