"""lightnovel-crawler adapters with all HTTP delegated to SCANCORE.

Adapted from the SCANCORE website's bridge. The desktop app starts one of
these per operation, answers its fetch requests itself (so cookies, limits
and browser-check clearance are shared with the app), and reads one result.
"""
import importlib
import importlib.util
import json
import os
import sys
from itertools import islice
from pathlib import Path
from urllib.parse import urlencode, urljoin, urlparse

try:  # Not available on Windows.
    import resource
    resource.setrlimit(resource.RLIMIT_AS, (512 * 1024 * 1024, 512 * 1024 * 1024))
    resource.setrlimit(resource.RLIMIT_CPU, (20, 20))
except (ImportError, ValueError, OSError):
    pass
VENDOR = Path(os.environ['SCANCORE_LNCRAWL_VENDOR'])
running = False

def audit(event, args):
    if event in {'socket.connect', 'socket.bind', 'subprocess.Popen', 'os.system'}:
        raise RuntimeError('Direct network and process access is disabled')
    if running and event == 'exec' and args[0].co_filename == '<string>':
        raise RuntimeError('Dynamic expression execution is unsupported')

sys.addaudithook(audit)
wire = sys.stdout
sys.stdout = sys.stderr  # Source print statements must not corrupt the protocol.
request = json.loads(sys.stdin.readline(32768))
from requests import Response
from scraper import PageSoup
from lncrawl.core import Crawler, LegacyCrawler, Novel, Chapter

def send(value):
    wire.write(json.dumps(value, ensure_ascii=False) + '\n')
    wire.flush()

class PublicScraper:
    def __init__(self, origin):
        self.origin = origin
        self.last_url = origin
        self.parser = 'html.parser'
        self.headers = {}
        self.cookies = {}
        self.session = self
        self.chapter_snapshot = ''
        self.crawler = None

    def request(self, method, url, **kwargs):
        url = urljoin(self.origin, str(url))
        if kwargs.get('params'):
            url += ('&' if '?' in url else '?') + urlencode(kwargs['params'], doseq=True)
        data = kwargs.get('data')
        headers = dict(self.headers)
        headers.update(kwargs.get('headers') or {})
        if 'json' in kwargs:
            data = json.dumps(kwargs['json'])
            headers['Content-Type'] = 'application/json'
        elif isinstance(data, dict):
            data = urlencode(data, doseq=True)
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
        send({'fetch': {'url': url, 'method': method, 'headers': headers, 'body': data}})
        answer = json.loads(sys.stdin.readline(4 * 1024 * 1024 + 32768))
        if answer.get('error'):
            raise RuntimeError(answer['error'])
        response = Response()
        response.status_code = answer['status']
        response.url = answer['url']
        response.headers.update(answer['headers'])
        response._content = answer['body'].encode('utf-8')
        response.encoding = 'utf-8'
        self.last_url = response.url
        response.raise_for_status()
        return response

    def get(self, url, **kwargs):
        return self.request('GET', url, **kwargs)

    def post(self, url, **kwargs):
        return self.request('POST', url, **kwargs)

    def get_json(self, url, **kwargs):
        return self.get(url, **kwargs).json()

    def post_json(self, url, **kwargs):
        return self.post(url, **kwargs).json()

    def make_soup(self, html, **kwargs):
        return PageSoup.create(str(html), parser=self.parser)

    def get_soup(self, url, **kwargs):
        soup = self.make_soup(self.get(url, **kwargs).text)
        selector = getattr(self.crawler, 'chapter_body_selector', '')
        if request['operation'] == 'chapter' and selector:
            content = soup.select_one(selector)
            if content:
                self.chapter_snapshot = str(content)
                # NovelFull sometimes closes #chapter-content before its body:
                # the actual public text is its next anonymous div sibling.
                if '#chapter-content' in selector and len(content.get_text()) < 500:
                    sibling = content.find_next_sibling('div')
                    if sibling and not sibling.get('id') and not sibling.get('class') and len(sibling.get_text()) > 500:
                        self.chapter_snapshot += str(sibling)
        return soup

    def post_soup(self, url, **kwargs):
        return self.make_soup(self.post(url, **kwargs).text)

    def close(self):
        pass

    def render_soup(self, *args, **kwargs):
        raise RuntimeError('This source requires browser rendering')

try:
    source = request['source']
    file = VENDOR / source['filePath']
    # A few upstream filenames contain dots; normal module import misreads them.
    module_name = source['module']
    try:
        module = importlib.import_module(module_name)
    except ModuleNotFoundError:
        package = source['filePath'].rsplit('/', 1)[0].replace('/', '.')
        spec = importlib.util.spec_from_file_location(package + '.scancore_adapter', file)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    if source['module'] == 'sources.en.r.royalroad':
        module.search_url = 'https://www.royalroad.com/fictions/search?title=%s'
    cls = getattr(module, source['className'])
    if not issubclass(cls, Crawler):
        raise RuntimeError('Invalid adapter')
    cls.max_workers = classmethod(lambda cls: 1)
    scraper = PublicScraper(request.get('origin') or source['origins'][0])
    crawler = cls(origin=scraper.origin, scraper=scraper)
    scraper.crawler = crawler
    # Subclasses can use their selected mirror without source updates or login.
    if isinstance(crawler, LegacyCrawler):
        crawler.home_url = scraper.origin
    running = True
    crawler.initialize()
    operation = request['operation']
    if operation == 'search':
        rows = [{'title': str(row['title']), 'url': str(row['url']), 'info': str(row.get('info') or '')} for row in islice(crawler.search(request['query']), 20)]
        send({'result': {'results': rows}})
    elif operation == 'book':
        novel = Novel(url=request['url'], volumes=[], chapters=[], tags=[])
        crawler.read_novel(novel)
        rows = [{'url': str(c['url']), 'title': str(c.get('title') or ''), 'volume': str(c.get('volume') or '')} for c in novel.chapters[:10000]]
        send({'result': {'title': novel.title, 'author': novel.author, 'cover': novel.cover_url, 'description': novel.synopsis, 'tags': list(novel.tags)[:100], 'chapters': rows, 'complete': len(novel.chapters) <= 10000}})
    elif operation == 'chapter':
        chapter = Chapter(id=1, url=request['url'], title=request.get('title', 'Chapter'))
        if isinstance(crawler, LegacyCrawler):
            crawler.novel_url = request['bookUrl']
        crawler.download_chapter(chapter)
        # Some template cleaners delete direct text inside div/br-based chapters.
        # Keep the adapter-selected public container; SCANCORE sanitizes and splits it.
        if scraper.chapter_snapshot:
            snapshot = PageSoup.create(scraper.chapter_snapshot)
            for tag in snapshot.select('script,style,noscript,iframe,form,[hidden]'):
                tag.decompose()
            chapter.body = str(snapshot)
        if not chapter.body:
            raise RuntimeError('No public chapter text was returned')
        send({'result': {'title': chapter.title, 'body': chapter.body}})
    else:
        raise RuntimeError('Unsupported operation')
except Exception as error:
    send({'failure': type(error).__name__})
    sys.exit(1)
