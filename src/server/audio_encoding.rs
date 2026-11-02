//! 16-bit PCM encodings of the float samples kokoro returns.

const WAV_HEADER_BYTES: usize = 44;

#[derive(Debug, PartialEq, Eq)]
pub struct WavTooLong;

impl std::fmt::Display for WavTooLong {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("audio too long for a WAV file")
    }
}

// Clamps to [-1, 1] and rounds half away from zero, like std::lround.
fn to_pcm16(sample: f32) -> i16 {
    (sample.clamp(-1.0, 1.0) * 32767.0).round() as i16
}

fn extend_pcm16(out: &mut Vec<u8>, samples: &[f32]) {
    out.extend(samples.iter().flat_map(|&s| to_pcm16(s).to_le_bytes()));
}

/// Raw signed 16-bit little-endian samples.
pub fn encode_pcm16(samples: &[f32]) -> Vec<u8> {
    let mut out = Vec::with_capacity(samples.len() * 2);
    extend_pcm16(&mut out, samples);
    out
}

/// A PCM16 mono WAV file.
pub fn encode_wav_pcm16(samples: &[f32], sample_rate_hz: u32) -> Result<Vec<u8>, WavTooLong> {
    let data_size = u32::try_from(samples.len())
        .ok()
        .and_then(|count| count.checked_mul(2))
        .filter(|size| size.checked_add(36).is_some())
        .ok_or(WavTooLong)?;
    let mut out = Vec::with_capacity(WAV_HEADER_BYTES + data_size as usize);
    out.extend_from_slice(b"RIFF");
    out.extend_from_slice(&(36 + data_size).to_le_bytes());
    out.extend_from_slice(b"WAVEfmt ");
    out.extend_from_slice(&16u32.to_le_bytes()); // fmt chunk size
    out.extend_from_slice(&1u16.to_le_bytes()); // WAVE_FORMAT_PCM
    out.extend_from_slice(&1u16.to_le_bytes()); // channels
    out.extend_from_slice(&sample_rate_hz.to_le_bytes());
    out.extend_from_slice(&sample_rate_hz.wrapping_mul(2).to_le_bytes()); // byte rate
    out.extend_from_slice(&2u16.to_le_bytes()); // block align
    out.extend_from_slice(&16u16.to_le_bytes()); // bits per sample
    out.extend_from_slice(b"data");
    out.extend_from_slice(&data_size.to_le_bytes());
    extend_pcm16(&mut out, samples);
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn u32_at(bytes: &[u8], offset: usize) -> u32 {
        u32::from_le_bytes(bytes[offset..offset + 4].try_into().unwrap())
    }

    fn u16_at(bytes: &[u8], offset: usize) -> u16 {
        u16::from_le_bytes(bytes[offset..offset + 2].try_into().unwrap())
    }

    #[test]
    fn pcm16_clamps_and_rounds_to_little_endian() {
        assert_eq!(encode_pcm16(&[0.0, 1.0, -1.0, 2.0]), b"\x00\x00\xff\x7f\x01\x80\xff\x7f");
        assert_eq!(encode_pcm16(&[0.5 / 32767.0, -0.5 / 32767.0]), b"\x01\x00\xff\xff");
    }

    #[test]
    fn wav_writes_a_pcm16_mono_header() {
        let samples = [0.0, 0.5, -0.5];
        let wav = encode_wav_pcm16(&samples, 24000).unwrap();
        assert_eq!(wav.len(), 44 + 6);
        assert_eq!(&wav[0..4], b"RIFF");
        assert_eq!(u32_at(&wav, 4), 36 + 6);
        assert_eq!(&wav[8..16], b"WAVEfmt ");
        assert_eq!(u16_at(&wav, 20), 1); // PCM
        assert_eq!(u16_at(&wav, 22), 1); // mono
        assert_eq!(u32_at(&wav, 24), 24000);
        assert_eq!(u32_at(&wav, 28), 48000);
        assert_eq!(u16_at(&wav, 34), 16);
        assert_eq!(&wav[36..40], b"data");
        assert_eq!(u32_at(&wav, 40), 6);
        assert_eq!(wav[44..], encode_pcm16(&samples));
    }
}
