% gen_lpf.m — синтез антиалиасингового FIR для 2 МГц -> 200 кГц (D=10)
fs = 2e6;  D = 10;
fp = 90e3;            % край пассбенда (полоса FM по Карсону)
fst = 110e3;          % край стопбенда = fs/D - fp  (что завернётся в полосу)
Rp = 0.1;             % пульсация пассбенда, дБ
Rs = 62;              % затухание стопбенда, дБ

% спека пульсаций в линейных единицах для оценки порядка
dp = (10^(Rp/20)-1)/(10^(Rp/20)+1);
ds = 10^(-Rs/20);

[n, fo, ao, w] = firpmord([fp fst], [1 0], [dp ds], fs);
b = firpm(n, fo, ao, w);     % порядок n -> (n+1) тапов
n = numel(b);

[H, f] = freqz(b, 1, 16384, fs);
pb = 20*log10(max(abs(H(f<=fp)))/min(abs(H(f<=fp))));
sb = 20*log10(max(abs(H(f>=fst))));
fprintf('N=%d  passband ripple=%.2f dB  stopband=%.1f dB  DCgain=%.4f\n', ...
        n, pb, sb, sum(b));
figure; plot(f/1e3, 20*log10(abs(H))); grid on;
xline(90); xline(110); yline(-Rs);
xlabel('kHz'); ylabel('dB'); title('LPF 2M\rightarrow200k');

this_dir = fileparts(mfilename('fullpath'));
out_path = fullfile(this_dir, '..', 'inc', 'dsp', 'filters', 'lowpass_2m_to_200k.hpp');

fid = fopen(out_path, 'w');
fprintf(fid, '#pragma once\n#include <array>\n\nnamespace dsp::filters {\n\n');
fprintf(fid, 'inline constexpr std::size_t kLowpass2mTo200kDecim = %d;\n', D);
fprintf(fid, 'inline constexpr std::array<float, %d> kLowpass2mTo200k = {{\n', n);
for i = 1:6:n
    fprintf(fid, '    ');
    fprintf(fid, '%+.8ef, ', b(i:min(i+5,n)));
    fprintf(fid, '\n');
end
fprintf(fid, '}};\n\n} // namespace dsp::filters\n');
fclose(fid);